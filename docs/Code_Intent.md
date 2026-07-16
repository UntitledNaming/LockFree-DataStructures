# 코드 의도 문서 (Code Intent) — P2 Lock-Free 자료구조

> 이 문서는 저장소의 모든 코드 파일(.h/.cpp, 재현·해결·테스트 코드)을 "무엇을 하는가"가 아니라 **"왜 이렇게 작성했는가"** 중심으로 설명한다.
> 동작 흐름은 `docs/Code_Flow.md`, 설계 비교는 `docs/Design_Rationale.md`, 문제 서사는 `docs/Troubleshooting.md` 참고.

---

## 0. 저장소 구조 자체의 의도

| 폴더 | 의도 |
|---|---|
| `include/` | 완성판. 다른 프로젝트(P1 네트워크 라이브러리)가 그대로 가져다 쓰는 배포 단위라 헤더 온리 템플릿으로 유지 |
| `issues/<문제>/reproduce` · `clear` | 문제마다 "터지는 코드"와 "고친 코드"를 **같은 파일명으로 쌍**으로 보존. diff만 보면 해결의 본질이 드러나게 하려는 의도 |
| `debug_cases/` | 문제를 좁혀가던 중간 산출물(로그, 분석 txt, 디버그 전용 클래스). 결과가 아니라 추적 과정을 증거로 남기려는 의도 |
| `test/LockFree_Spinlock_SRWLock_comparetest/` | "락프리가 항상 빠른가"를 같은 측정 프로그램에서 측정하기 위한 비교 전용 프로젝트 |

---

## 1. include/ — 완성판 자료구조

### 1.1 `include/LFStack.h` — `LFStack<T>`

핵심 아이디어: top 포인터의 **최상위 17bit에 버전 태그**를 실어 ABA를 감지한다. 64bit Windows에서 유저 주소 공간이 이 비트를 쓰지 않는다는 사실을 이용했다.

| 함수 | 하는 일 | 이렇게 작성한 의도 |
|---|---|---|
| `LFStack(int size)` | `GetSystemInfo`로 유저 주소 범위 검사 후 실패 시 `__debugbreak`, 메모리 풀 생성 | 17bit 태그의 전제(주소 상위 비트 미사용)는 OS 버전에 따라 깨질 수 있다. 전제가 깨지면 조용히 오동작하는 대신 **시작 시점에 즉시 죽게** 만들었다 |
| `~LFStack()` | 풀 삭제 | 노드 수명은 전부 풀 소관이므로 스택 자신은 풀만 정리하면 된다 |
| `Push(T)` | 태그 생성 → do-while: 태그 제거 → top 저장 → next 연결 → 태그 부착 → CAS | CAS 실패 시 재시도 루프 첫 줄에서 태그를 떼는 이유: 이전 시도에서 붙인 태그가 남아 있으면 `pNextNode` 참조 자체가 잘못된 주소를 읽는다 |
| `Pop(T&)` | top 저장 → 실주소 복원 → next에 태그 부착 → CAS → 데이터 반환 → `Free` | Pop도 top을 바꾸는 행위이므로 태그를 새로 발급한다. 반납은 `delete`가 아닌 풀 `Free` — Page Decommit 차단이 목적 |
| `Clear()` | top/size/태그 카운트 초기화(풀 유지) | 스택 재활용 시 풀까지 다시 만들 필요가 없어서 분리 |
| `IsEmpty()` / `GetUseCnt()` | 상태 조회 | 측정·검증 코드에서 상태를 확인하기 위한 보조 |
| `struct Node { T data; Node* pNextNode; }` | 노드 정의 | 데이터를 노드에 직접 담아(포인터 아님) 풀과의 캐스팅 호환을 유지 |

### 1.2 `include/LFQSingleLive.h` — `LFQueue<T>` (풀 단독 사용판)

| 함수 | 하는 일 | 이렇게 작성한 의도 |
|---|---|---|
| 생성자 | 더미 노드 1개 생성, head/tail이 더미를 가리킴 | 빈 큐 상태 전이에서 head/tail 두 변수를 원자적으로 함께 바꿀 방법이 없어, "더미만 있음 = 빈 큐"로 정의해 문제 자체를 제거 (`Design_Rationale.md` §9) |
| `Enqueue(T)` | 노드 할당 후 `_next = 0xFFFF...F` → tail 밀기 사전 작업 → 1st CAS(링크) → next를 nullptr로 복원 → 2nd CAS(tail 전진) | next를 nullptr가 아닌 **표식 값로 초기화**하는 것이 Order Reversal 해결의 핵심. "next==nullptr = 완성된 꼬리"라는 의미가 삽입 중 노드와 겹치지 않게 했다 |
| `Dequeue(T&)` | tail 밀기 사전 작업 → head 태그 CAS 전진 → `headNext->_data` 반환 → 옛 head를 풀에 반납 | Dequeue에서도 tail을 먼저 미는 이유: 2nd CAS 실패로 뒤처진 tail을 다음 연산이 보정하는 구조이기 때문 (`Design_Rationale.md` §10) |
| `Clear()` | size/태그 초기화 | 큐 재활용용. 노드 정리는 풀 소관 |

### 1.3 `include/LFQMultiLive.h` — `LFQueueMul<T>` (공유 풀 대응판)

| 함수/구조체 | 하는 일 | 이렇게 작성한 의도 |
|---|---|---|
| 생성자 | refCnt 0→1 스레드만 static 풀 생성, 나머지는 생성 완료까지 스핀 대기. `m_Qid = (UINT64)&m_size` | 타입별 공유 풀이므로 생성 경쟁을 참조 카운트로 단일화. Qid는 별도 발급기 없이 **멤버 주소값**을 쓰면 프로세스 내 고유성이 공짜로 보장된다 |
| 소멸자 | refCnt 감소, 0이면 풀 삭제 | 마지막 큐가 풀을 정리 — 공유 자원의 수명 규칙을 명시 |
| `Enqueue(T)` | 사전 작업 후 1st CAS를 `InterlockedCompareExchange128(&next, {nullptr, 내 Qid})`로 수행 | 공유 풀에서는 "next==nullptr"만으로는 **남의 큐로 재사용된 노드**를 구분할 수 없다. 링크와 소유 검증을 한 번의 원자 연산으로 묶기 위해 128bit CAS를 썼다 |
| `Dequeue(T&)` | 단일판과 동일 구조 | Dequeue는 head만 다루므로 Qid 검증이 필요 없다 — 필요한 곳에만 비용을 지불 |
| `struct Node { T _data; alignas(16) Node* _next; UINT64 _Qid; }` | 노드 정의 | CAS128 대상(_next, _Qid)이 16바이트 경계에 있어야 해서 `alignas(16)` |
| `struct CmpNode { Node* s_next; UINT64 s_Qid; }` | CAS128 비교값 | 비교 대상 레이아웃을 노드의 (next, Qid)와 정확히 일치시키기 위한 전용 구조체 |

### 1.4 `include/LockFreeMemoryPoolLive.h` — `CMemoryPool<T>`

존재 이유: Page Decommit 해결. 노드를 OS 힙에 돌려주지 않고 풀 안에 보존해, 참조 중 페이지가 사라지는 상황을 구조적으로 차단한다.

| 함수 | 하는 일 | 이렇게 작성한 의도 |
|---|---|---|
| 생성자 | `poolID = &m_pTopNode`, iBlockNum만큼 사전 생성 | ID를 멤버 주소로 쓰는 이유는 Qid와 동일(고유성 공짜). 0이면 프리 리스트 방식, 양수면 사전 확보 — 사용처가 부하 특성에 맞게 고르게 함 |
| `Alloc()` | 사용량==용량이면 `CPushBack`으로 새 노드 즉시 반환, 아니면 태그드 CAS로 top 분리 | 부족 시 스택에 연결했다 다시 빼는 왕복을 생략하고 **바로 외부로** 전달 — 경합 구간을 줄이려는 선택 |
| `Free(T*)` | `T*`를 Node*로 캐스팅 → `s_poolD != m_iOriginID`면 false → placement면 소멸자 호출 → 태그드 CAS로 top 연결 | T가 노드의 첫 멤버라 캐스팅만으로 노드 주소가 나온다. poolID 검사는 **다른 풀의 노드가 섞여 들어오는 사고**(공유 풀 오염의 풀 버전)를 반납 시점에 잡기 위함 |
| `PushBack()` / `PushBack(N)` / `CPushBack(Node**)` | 노드 생성·연결(단일/다중/외부 직행) | 초기화 구간(단일 스레드)용과 운영 구간(CAS 필요)용을 분리해 초기화에 불필요한 원자 연산을 없앰 |
| `GetCapacityCnt()` / `GetUseCnt()` | 용량/사용량 조회 | 누수 관측용 지표 |
| 소멸자 | 스택 순회하며 (placement 아니면 소멸자 호출 후) `free` | 보존했던 노드를 이때만 OS로 돌려준다 — "운영 중 미반환" 원칙의 종료 시 예외 |

---

## 2. issues/ — 문제 재현·해결 쌍

공통 의도: **재현 없는 해결은 증거가 없다.** 각 문제를 결정적으로 터뜨리는 최소 코드를 만들고, 같은 시나리오에서 해결판이 통과함을 보인다.

### 2.1 ABA (`issues/ABA/`)

| 파일 | 의도 |
|---|---|
| `reproduce/LFStack_ver1.h` | **태그 없는 raw CAS64** Push/Pop — ABA가 실제로 나는 조건을 그대로 보존. `StackCapture`/`FileLog`로 시도마다 스레드ID·top·newTop을 기록해, 크래시 후 로그 index를 역추적할 수 있게 계측을 심음 |
| `reproduce/CMemoryPoolByQueue.h` | CriticalSection으로 보호한 **큐 방식** 풀. 큐(FIFO)를 쓴 이유: 반납 순서대로 재할당되므로 "방금 반납된 주소가 곧 재사용"되는 ABA 조건을 결정적으로 만들 수 있다. `Free`에서 front와 비교해 이중 반납을 감지하는 검증 장치 포함 |
| `reproduce/main.cpp` | 2스레드가 Push/Pop을 반복하는 최소 시나리오 | 
| `clear/LFStack.h`(및 `LFStack_ver2.h`) | 동일 시나리오에 태그드 포인터 판을 투입 — reproduce와의 diff가 곧 해결책 |

### 2.2 Page Decommit (`issues/PageDecommit/`)

| 파일 | 의도 |
|---|---|
| `reproduce/LFStack_ver2.h` | `VirtualAlloc`으로 페이지를 직접 잡아 노드 256개(16B×256=1페이지)를 배치하고, 전부 반납되는 순간 `VirtualFree(MEM_DECOMMIT)`를 **코드가 직접** 호출 | 
| — 왜 직접 디커밋하나 | delete만으로는 힙이 페이지를 언제 반환할지 통제할 수 없어 재현이 비결정적이었다. 페이지 수명을 테스트 코드가 쥐어야 "한 스레드가 top을 본 순간 페이지가 사라지는" 교차를 확실히 만들 수 있다 |
| `reproduce/main.cpp` | 스레드 1은 첫 Pop에서 top만 저장한 채 대기, 스레드 2가 256개 전부 Pop → 디커밋 → 스레드 1 재개 시 참조 오류 |
| `clear/LFStack.h` + `LockFreeMemoryPoolLive.h` | 같은 시나리오를 자체 풀 판으로 — 페이지가 반환되지 않으므로 통과 |

### 2.3 Order Reversal (`issues/OrderReversal/`)

| 파일 | 의도 |
|---|---|
| `reproduce/LFQSingleLive.h` | `Enqueue`가 새 노드의 `_next = nullptr`로 시작하는 판 — 재사용 노드의 next 초기화 순간 1st CAS가 오통과하는 원인을 보존 |
| `clear/LFQSingleLive.h` | `_next = 0xFFFF...F` 표식 값 판. **reproduce와의 diff가 한 줄 수준**이 되도록 나머지를 동일하게 유지 — "무엇이 문제였나"를 코드로 증명하는 구성 |
| `reproduce/main.cpp` | 3스레드 Enqueue/Dequeue 혼합. 한 스레드의 삽입 순서(1000→1001)가 Dequeue에서 뒤집히는 것을 값으로 관측 |

### 2.4 Shared Pool (`issues/SharedPool/`)

| 파일 | 의도 |
|---|---|
| `reproduce/LFQSingleLive.h` | Qid 없는 CAS64 판 + static 공유 풀 — 큐1의 tail로 봤던 노드가 큐2로 재사용될 때 오염되는 조건 보존 |
| `reproduce/main.cpp` | 큐 2개, 값 대역 분리(큐1: 0~999 / 큐2: 1000~). 큐2에서 큐1 대역 값이 나오면 오염 확정 — **버그 판정을 값 대역으로 자동화**하려는 설계 |
| `clear/LFQMultiLive.h` | Qid+CAS128 판. 남의 큐 노드면 1st CAS가 실패하도록 |

---

## 3. debug_cases/ — 추적 과정 보존

| 항목 | 의도 |
|---|---|
| `ABA/` (LFStack_ver1.h + main.cpp + 로그 분석 txt) | 크래시 시점 로그 index부터 거꾸로 읽어 두 스레드의 교차를 복원한 원본. "재현→분석" 사이의 실제 작업 증거 |
| `Page_Decommit/` | 디커밋 재현 환경을 좁혀가던 중간판 |
| `lockfreequeue_debug_project/` (`DumpClass.h`, `LockFreeQ.h` 등) | 2nd CAS 실패를 12차례 로그를 바꿔가며 분석할 때 쓰던 디버그 전용 큐/덤프 클래스. `DumpClass`는 크래시 순간 스택을 캡쳐해 **다른 스레드에서** 파일로 남기기 위한 장치(크래시 스레드에서 파일 IO는 위험) |

---

## 4. test/ — 동기화 방식 비교 측정 프로그램

의도: "락프리가 항상 빠른가"라는 질문에 **같은 측정 프로그램, 같은 작업량, 같은 지표**로 답하기 위한 전용 프로젝트. 자료구조 코드가 아니라 측정 코드가 주인공이다.

| 파일 | 함수 | 의도 |
|---|---|---|
| `main.cpp` | `main` | `CTest` 생성 → `TestInit` → `ThreadCreate` → `TestClear`의 뼈대만 남김. 진입점에 로직을 두지 않아 테스트 구성이 `CTest` 안에서 한눈에 보이게 |
| `Test.h/.cpp` | `TestInit` | 스레드 수/동기화 타입/측정 시간을 입력받아 자료구조 초기화 — 조건을 코드 수정 없이 바꾸기 위한 매개변수화 |
| | `ThreadCreate` | 테스트 스레드 N + 모니터 스레드 1 분리 — 측정 대상과 지표 수집을 다른 스레드로 격리 |
| | `StackTest(idx)` | 타입별(SRWLock/SpinLock/LockFree) Push/Pop 루프. 같은 루프 골격에 동기화만 갈아 끼우는 구조 — **비교 대상 외 변수를 통제**하려는 핵심 장치. 측정 지점이 정확하다: `__rdtsc()`를 락 획득 직전/직후에 찍어 스레드별 `m_usertime[idx]`에 누적 — 벽시계가 아니라 사이클 단위로, "락 획득에 든 비용"만 분리 측정 |
| | `SpinLock/SpinUnlock(idx)` | `InterlockedExchange` 스핀 직접 구현 + 획득 성공/실패 카운팅 — 라이브러리 스핀락 대신 직접 구현해 카운팅 지점을 확보 |
| | `MonitorThread` | PDH로 CPU·컨텍스트 스위치 수집 — 처리량만으로는 못 보는 비용(스핀의 CPU 소모)을 함께 기록 |
| | `FileStore` | Total Count·성공/실패 비율·CPU를 파일로 기록 — 결과를 사람이 다시 계산하지 않도록 지표 산출까지 코드가 담당 |
| `LFStack.h` (계측판) | `Push(T, idx, INTERLOCKCNT*)` / `Pop(T, idx, INTERLOCKCNT*)` | include 판과 로직은 같고, **CAS 루프 안에** total/success/fail 카운트를 심은 판. 계측을 완성판에 넣지 않고 사본에 넣은 이유: 배포용 자료구조에 측정 오버헤드를 남기지 않기 위해 |
| `Interlocked.h` | `INTERLOCKCNT` | 스레드별 total/success/fail 슬롯 — 공유 카운터 경합이 측정을 오염시키지 않도록 스레드별로 분리 |
| `Stack.h` | `Stack<T>` (비-락프리) | 락으로 감싸는 비교군. 락프리와 같은 인터페이스로 맞춰 측정 프로그램가 동일 코드로 돌 수 있게 |
| `ProcessMonitor.h/.cpp`, `CPUUsage.h/.cpp` | `UpdateCounter` 등 | PDH 카운터(프로세스 메모리·CPU) 래핑 — P1 모니터링 코드와 같은 계열로, 측정 인프라 재사용 |
| `ProfilerTLS.h` | RAII 프로파일러 | P3와 공유하는 측정 인프라. TLS 기반이라 멀티스레드 측정에서 락 없이 구간 시간을 기록 |
| `DumpClass.h` | 덤프 캡쳐 | 테스트 중 크래시 시 원인 보존용 |

---

## 5. 확인 필요

| 항목 | 내용 |
|---|---|
| `debug_cases` 분석 txt 원문 | 12케이스 분석 로그는 대표 케이스만 정독됨. 전체 판독은 미완 |
| `docs/results` 실측값 | 스레드 1/3/16 Total Count 수치는 txt 미파싱 상태 — 문서·발표 인용 전 확정 필요 |
| `PageDecommit/clear` 빌드 구성 | `LFStack_ver2.h` 포함 vs 실제 링크 대상 vcxproj 확인 |
