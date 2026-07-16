# Code Flow

이 문서는 Lock-Free 자료구조와 4대 동시성 문제의 코드 흐름을 기능 기준으로 정리합니다. README보다 자세하되, 함수 전체 목록이 아니라 각 흐름의 핵심 함수만 설명합니다. 각 문제는 `issues/<문제>/{reproduce, clear}/`에 재현 코드와 해결 코드가 쌍으로 정리되어 있고, 완성 버전은 `include/`에 있습니다.

공통 기법 두 가지를 먼저 정리합니다.

- **태그드 포인터(tagged pointer)**: 유저 모드 주소는 상위 비트가 비어 있으므로, 포인터 상위 비트에 단조 증가 카운터를 실어 CAS 대상에 버전을 부여합니다. 실제 주소는 비트 마스크로 복원합니다.
- **소유 식별 + CAS128**: 여러 자료구조가 노드 풀을 공유할 때, 노드에 소유 큐 식별자(Qid)를 심고 링크를 128비트 CAS로 바꾸며 `(next, Qid)`를 함께 비교합니다.

---

## 1. Lock-Free Stack Push / Pop 흐름

- 목적: 락 없이 스택 Push/Pop을 CAS로 구현하되 ABA를 태그로 막는다.
- 관련 파일: `include/LFStack.h`, `include/LockFreeMemoryPoolLive.h`
- 관련 클래스: `LFStack<T>`(내부 `Node`), `CMemoryPool<T>`
- 관련 함수: `LFStack::Push`, `LFStack::Pop`, `CMemoryPool::Alloc/Free`
- 처리 순서:
  1. Push: 메모리 풀에서 노드를 할당하고 데이터를 넣는다. 이번 시도의 태그 카운터를 확보한다.
  2. top을 읽고(마스크로 실제 주소 복원), 새 노드의 next를 기존 top으로 연결한다.
  3. 새 노드에 태그를 얹어 top에 대해 CAS한다. 실패하면 태그를 벗기고 top을 다시 읽어 재시도한다.
  4. Pop: top을 읽어 실제 주소를 복원하고(비어 있으면 false), 다음 노드에 태그를 얹어 top에 CAS한다. 실패 시 재시도.
  5. Pop 성공 시 데이터를 꺼내고 노드를 `CMemoryPool::Free`로 풀에 반환한다(OS로 반환하지 않음).
- 문제 발생 조건: 태그 없이 raw CAS만 쓰면 Pop 도중 노드가 재사용되어 ABA가 발생한다(3번 흐름).
- 해결 방식: top 포인터에 단조 증가 태그를 실어 "같은 주소 + 같은 버전"을 요구한다.
- 관련 테스트: `test/`의 Lock-Free 동작·비교 테스트.
- 관련 트러블슈팅: ABA 문제.
- 확인 필요: 없음(코드 확인).

---

## 2. Lock-Free Queue Enqueue / Dequeue 흐름

- 목적: 락 없이 큐를 CAS로 구현하고, 삽입 중 노드로 인한 순서 꼬임과 공유 풀 오염을 막는다.
- 관련 파일: `include/LFQSingleLive.h`(단일 프로듀서 `LFQueue`), `include/LFQMultiLive.h`(멀티 프로듀서 `LFQueueMul`)
- 관련 클래스: `LFQueue<T>` / `LFQueueMul<T>`(내부 `Node`, `CmpNode`), `CMemoryPool<T>`
- 관련 함수: `Enqueue`, `Dequeue`
- 처리 순서(Enqueue):
  1. head/tail은 더미 노드 1개로 초기화한다(빈 큐 판정을 단순화). head/tail 포인터에는 카운터 태그를 실는다.
  2. 새 노드를 할당하고 next를 삽입 진행 중을 뜻하는 특정 값(예: `0xFFFF...`)으로 초기화한다. 멀티 버전은 노드에 소유 큐 식별자(Qid)도 세팅한다.
  3. tail 전진 사전작업: tail의 next가 nullptr이면 진짜 꼬리, 삽입 진행 값이면 남이 삽입 중이므로 대기(continue), 그 외면 뒤처진 tail을 next로 전진시킨다.
  4. 링크: 단일 버전은 tail의 next에 CAS64로 새 노드를 건다. **멀티 버전은 `(next==nullptr, Qid==내큐)`를 128비트 CAS로 함께 검사**한다.
  5. 링크 성공 후 새 노드의 next를 nullptr로 확정하고 tail을 새 노드로 전진시킨다.
- 처리 순서(Dequeue):
  1. tail 사전작업으로 뒤처진 tail을 정리한다.
  2. head를 읽어(실제 주소 복원) 다음 노드가 없으면 빈 큐(false).
  3. head를 다음 노드로 태그를 얹어 CAS 전진한다. 성공 시 다음 노드의 데이터를 꺼낸다(더미 다음 노드).
  4. 기존 더미 노드를 풀에 반환한다. 다음 노드가 새 더미가 된다.
- 문제 발생 조건: 삽입 진행 중 노드를 nullptr로 두면 순서가 꼬이고(5번), 여러 큐가 풀을 공유하면 남의 노드가 끼인다(6번). Pop/Dequeue 후 노드 반환 타이밍에 다른 스레드가 참조 중일 수 있다.
- 해결 방식: 삽입 진행 값 표식 값로 순서 보호, Qid+CAS128로 소유 검증, 자체 풀로 노드 보존.
- 관련 테스트: `test/` 및 각 문제 재현/해결.
- 관련 트러블슈팅: Order Reversal, Shared Node Pool 오염.
- 확인 필요: Dequeue의 데이터 취득 지점(전진한 노드) 등 미세 지점은 구현부 재확인 권장.

---

## 3. ABA 재현 / 해결 흐름

- 목적: 태그 없는 CAS 스택에서 ABA를 재현하고 태그드 포인터로 해결한다.
- 관련 파일: `issues/ABA/reproduce/{main.cpp, LFStack_ver1.h, CMemoryPoolByQueue.h}`, `issues/ABA/clear/{LFStack.h, LFStack_ver2.h, main.cpp}`, `debug_cases/ABA/`
- 관련 클래스: `LFStack<T>`(재현판=태그 없음, 해결판=태그드), 메모리 풀
- 관련 함수: `Push/Pop`, `StackCapture`(로그 기록), `FileLog`, `CMemoryPool::Free`(소유 ID 검사)
- 처리 순서(재현):
  1. 2개의 스레드가 `Push 10 → Pop 10 → Push 0`을 무한 반복해 노드를 계속 재사용한다.
  2. 재현판 Pop은 `t = top; next = t->next; CAS(top, next, t)` — 태그 없는 raw CAS64.
  3. `StackCapture`가 각 시도의 지역 top/next와 스레드 ID·순번을 링 버퍼에 기록한다.
  4. Free에서 소유 ID 불일치나 이중 반납이 감지되면 로그를 파일로 남기고 중단한다.
- 문제 발생 조건: 스레드 A가 top과 top->next를 읽은 사이, 스레드 B가 그 노드를 Pop→반환→다시 Push해서 top이 A→B→A로 돌아온다. A의 CAS는 주소가 같으면 성공하지만 next는 이미 낡은 값이라 리스트가 깨진다.
- 해결 방식: top 포인터에 단조 증가 태그(버전 카운터)를 실어, 주소가 같아도 버전이 다르면 CAS가 실패하게 한다. version counter를 64비트 포인터 안에 태그드 포인터 형태로 포함한다(별도 128비트 없이).
- 로그 분석 방식: 현재 상태만으로는 원인이 안 보이므로, 각 스레드의 시도를 순번과 함께 링 버퍼에 남기고 크래시 순간의 시퀀스를 되짚는다.
- 관련 테스트: ABA 재현/해결 테스트.
- 관련 트러블슈팅: ABA 문제.
- 확인 필요: 재현용 풀(`CMemoryPoolByQueue.h`) 구현 세부는 미열람.

---

## 4. Page Decommit 재현 / 해결 흐름

- 목적: 노드 메모리가 OS에 반환됐을 때 다른 스레드가 그 주소를 참조하는 크래시를 재현하고, 자체 풀로 해결한다.
- 관련 파일: `issues/PageDecommit/reproduce/{main.cpp, LFStack_ver2.h}`, `issues/PageDecommit/clear/{main.cpp, LFStack.h, LockFreeMemoryPoolLive.h}`
- 관련 클래스: `LFStack<T>`(재현판=VirtualAlloc 배열, 해결판=자체 풀 기반)
- 관련 함수: `Push/Pop`, (재현) `Clear`의 `VirtualAlloc`, `Pop`의 `VirtualFree(MEM_DECOMMIT)`
- 처리 순서(재현):
  1. 재현판은 노드 배열 페이지를 `VirtualAlloc`으로 직접 확보한다.
  2. 노드를 채운 뒤 2개의 스레드가 경쟁적으로 Pop한다.
  3. Pop에서 마지막 노드 반환 시점이 되면 `VirtualFree(..., MEM_DECOMMIT)`로 페이지를 OS에 반환한다.
- 문제 발생 조건: 스레드 A가 이미 top을 읽고 top->next를 역참조하려는 순간, 스레드 B가 페이지를 반환하면 A가 해제된 페이지에 접근해 크래시한다.
- ABA 카운터로 막을 수 없는 이유: 버전 카운터는 "같은 주소 재사용"을 막을 뿐, "그 주소가 아직 유효한 메모리인지"는 보장하지 못한다. 메모리가 OS로 사라지면 어떤 카운터도 소용없다.
- 해결 방식: 노드를 OS로 되돌리지 않는 자체 메모리 풀(회수 보류)을 사용한다. 해결판 main은 자체 풀 기반 스택을 사용해 같은 시나리오를 크래시 없이 통과한다.
- 관련 테스트: Page Decommit 재현/해결 테스트.
- 관련 트러블슈팅: Page Decommit 문제.
- 확인 필요: 해결판이 어떤 구현을 실제로 링크하는지 프로젝트 설정 재확인 권장(해결판 main은 자체 풀 기반 스택을 include).

---

## 5. Order Reversal 재현 / 해결 흐름

- 목적: 큐 삽입 중 노드를 다른 스레드가 오인해 발생하는 순서 꼬임을 재현하고, 표식 값로 해결한다.
- 관련 파일: `issues/OrderReversal/reproduce/{main.cpp, LFQSingleLive.h}`, `issues/OrderReversal/clear/{main.cpp, LFQSingleLive.h}`, `debug_cases/2nd CAS 실패 문제점 분석/`
- 관련 클래스: `LFQueue<T>`(단일 프로듀서)
- 관련 함수: `Enqueue`, `Dequeue`
- 처리 순서:
  1. 단일 큐에 여러 스레드가 Enqueue/Dequeue를 섞어 수행한다.
  2. 재현판 Enqueue는 새 노드의 next를 nullptr로 시작하고, tail 전진 사전작업이 next가 nullptr인 경우만 종료 조건으로 본다.
- 문제 발생 조건: 새 노드가 next=nullptr인 상태로 tail에 링크되는 도중, 다른 스레드가 tail의 next가 nullptr인 것을 보고 그 노드를 "완성된 꼬리"로 착각한다. 그러면 자기 노드를 붙이거나 tail을 전진시켜 삽입이 겹치고 순서가 뒤집히거나 노드가 유실된다(이른바 2nd CAS 경쟁).
- 해결 방식: 삽입 중 노드의 next를 nullptr이 아닌 삽입 진행 값(표식 값)으로 초기화하고, 사전작업이 그 값을 보면 대기(continue)한다. 링크 CAS 성공 후에야 next를 nullptr로 확정 삽입한다. 재현→해결의 실제 diff는 (1) next 초기값을 nullptr→표식 값, (2) 종료 조건에 표식 값 분기 추가, (3) 링크 성공 후 next=nullptr 확정이다.
- 배운 점: "next가 nullptr"이 곧 "완성된 꼬리"를 뜻하지 않는다. 삽입 중 상태를 명시해야 다른 스레드가 개입하지 못한다.
- 관련 테스트: Order Reversal 재현/해결 테스트.
- 관련 트러블슈팅: Order Reversal 문제.
- 확인 필요: `debug_cases/2nd CAS 실패 문제점 분석/`의 케이스 원문은 미판독.

---

## 6. Shared Node Pool 오염 재현 / 해결 흐름

- 목적: 여러 큐가 하나의 노드 풀을 공유할 때 남의 노드가 끼는 오염을 재현하고, Qid+CAS128로 해결한다.
- 관련 파일: `issues/SharedPool/reproduce/{main.cpp, LFQSingleLive.h}`, `issues/SharedPool/clear/{main.cpp, LFQMultiLive.h}`
- 관련 클래스: 재현판=`LFQueue`(단일, Qid 없음), 해결판=`LFQueueMul`(Qid+CAS128)
- 관련 함수: `Enqueue`의 링크 CAS, `Dequeue`, 메모리 풀 Alloc/Free
- 처리 순서(재현):
  1. `LFQueue<int>` 두 개를 만든다. 노드 풀이 타입별 정적(static)이라 두 큐가 같은 풀을 공유한다.
  2. 스레드들이 한 큐에서 뺀 노드를 다른 큐에 넣는 식으로 노드가 두 큐를 오간다.
  3. 재현판의 링크는 `_next`에 대한 CAS64 — 소유 검증(Qid)이 없다.
- 문제 발생 조건: 큐1이 반환한 노드를 큐2가 재사용하는 중, 큐1의 tail CAS가 그 노드를 여전히 자기 것으로 보고 링크하면 한 노드가 두 큐에 걸치는 오염이 생긴다. 64비트 CAS는 주소만 비교하므로 소유가 바뀐 것을 못 잡는다.
- 해결 방식: `LFQueueMul`이 노드에 소유 큐 식별자(Qid)를 심고, 링크를 128비트 CAS로 바꿔 `(next==nullptr && Qid==내큐)`를 함께 검사한다. 남의 큐 노드면 CAS가 실패해 거부된다. 해결판 main은 `LFQueueMul` 두 개를 사용한다.
- Qid / owner 검증 여부: 해결판은 Qid로 소유를 검증한다.
- CAS128 사용 여부: 해결판 링크에서 사용한다.
- Enqueue와 Dequeue에서 태그 필요 여부: 실제 코드 기준으로, 멀티 버전은 head/tail 포인터 CAS에 카운터 태그를 Enqueue(tail)와 Dequeue(head, tail) 모두에 적용하고, 노드 링크(`_next`)에는 태그 대신 Qid+CAS128을 사용한다. 즉 "노드 링크 오염"은 태그가 아니라 Qid로 막는다.
- 관련 테스트: Shared Node Pool 오염 재현/해결 테스트.
- 관련 트러블슈팅: Shared Node Pool 오염 문제.
- 확인 필요: 일부 정리 노트의 "Enqueue에는 태그가 불필요하고 Dequeue에서 필요하다"는 표현은 위 코드(둘 다 head/tail 태그 적용)와 뉘앙스가 달라, 스택 태그와 큐 헤드/테일 태그·Qid를 구분해 재정리가 필요하다.

---

## 7. 동기화 방식 비교 테스트 흐름

- 목적: 같은 작업을 Spinlock / SRWLock / Lock-Free로 돌려 처리량과 경합 특성을 스레드 수별로 비교한다.
- 관련 파일: `test/LockFree_Spinlock_SRWLock_comparetest/{Test.cpp, Test.h, Stack.h, LFStack.h}`, 결과 `docs/results/`
- 관련 클래스: `CTest`, `Stack<T>`(비-락프리, 락으로 보호), `LFStack<T>`(락프리)
- 관련 함수: `CTest::TestInit/ThreadCreate/StackTest/SpinLock/SpinUnlock/FileStore`
- 처리 순서:
  1. 스레드 수·테스트 타입(SRWLock/SpinLock/Lock-Free)·시간을 입력받고 자료구조를 초기화한다.
  2. 테스트 스레드 N개와 모니터 스레드 1개를 만든다.
  3. 각 스레드가 정해진 시간 동안 `Push + Pop`을 반복하며 처리 횟수를 카운팅한다(Total Count).
     - SRWLock: 락으로 스택을 보호하고 락 획득 시간을 측정.
     - SpinLock: 원자적 교환으로 스핀하며 시도/성공/실패를 카운팅.
     - Lock-Free: 락 없이 CAS로 진행(내부에서 시도/성공/실패 카운팅).
  4. 모니터 스레드가 CPU·컨텍스트 스위치 등을 수집한다.
  5. `FileStore`가 Total Count, 시도/성공/실패 비율, CPU 등을 파일로 기록한다. 파일명에 방식·스레드 수·시간이 들어간다.
- 문제 발생 조건(성능 관점): 스레드가 늘수록 락 경쟁·컨텍스트 스위치가 늘고, 락프리는 CAS 실패 재시도가 비용이 된다.
- 해결 방식(관점): 방식별 처리량을 측정으로 비교해, 어느 하나가 항상 우위가 아니라 상황에 따라 선택해야 함을 확인한다.
- 관련 테스트: 스레드 1/3/16 등 비교(`docs/results/`).
- 관련 트러블슈팅: 비교 측정 이슈.
- 확인 필요: `test/`의 Lock-Free 스택은 계측 인자를 받는 버전으로 `include/`판과 시그니처가 다르다. `docs/results/`의 실측 수치는 미판독.

> 각 흐름을 그렇게 설계한 이유는 `docs/Design_Rationale.md`, 문제 해결 과정은 `docs/Troubleshooting.md`, 측정은 `docs/Test_Report.md`를 참고하세요. 재현/해결 영상 링크는 `docs/videos/`에 참고용으로 있습니다.
