# Build & Run

이 문서는 README보다 자세한 빌드·실행 안내입니다. 이 레포에는 **고의로 크래시나 동시성 문제를 재현하는 테스트**가 포함되어 있으므로, 재현 테스트와 해결 테스트를 구분해 실행해야 합니다.

---

## 1. 개발 환경

- OS: Windows
- IDE: Visual Studio 2022
- 언어 표준: C++17
- 빌드 구성: x64
- 사용 API: Windows의 Interlocked 계열 원자 연산을 사용합니다. 대표적으로 64비트 CAS(`_InterlockedCompareExchange64`)와 128비트 CAS(`InterlockedCompareExchange128`), 증감(`InterlockedIncrement` 등)을 사용합니다. Page Decommit 재현 코드는 `VirtualAlloc` / `VirtualFree`(MEM_DECOMMIT)를 사용합니다.


---

## 2. 빌드 방법

- 재현/해결 코드는 `issues/<문제>/{reproduce, clear}/` 아래에, 동기화 비교 테스트는 `test/LockFree_Spinlock_SRWLock_comparetest/`에, 완성 버전 헤더는 `include/`에 있습니다.
- 각 테스트를 Visual Studio 2022로 열어 x64로 빌드합니다.
- Release로 빌드.
- 빌드 대상: 확인하려는 시나리오(예: ABA 재현, ABA 해결, 동기화 비교)에 해당하는 프로젝트.

---

## 3. 테스트 실행

- **Lock-Free Stack / Queue 동작 확인**: `include/`의 완성 버전을 사용하는 테스트(또는 `test/`)로 다수 스레드에서 정상 동작을 확인합니다.
- **문제 재현 테스트**: `issues/<문제>/reproduce/`를 실행합니다. 의도적으로 ABA·Page Decommit·Order Reversal·공유 풀 오염을 일으키는 코드이므로 크래시나 이상 동작이 나타날 수 있습니다.
- **문제 해결 테스트**: `issues/<문제>/clear/`를 실행합니다. 같은 시나리오에서 문제가 재현되지 않아야 합니다.
- **동기화 방식 비교 테스트**: `test/`를 실행하고, 스레드 수·테스트 타입(Spinlock/SRWLock/Lock-Free)·시간을 입력해 측정합니다.

---

## 4. 결과 확인

- **Total Count 로그**: 동기화 비교 테스트는 방식·스레드 수·시간이 파일명에 들어간 결과 파일에 Total Count, 시도/성공/실패 비율, CPU 등을 기록합니다. `docs/results/` 아래에 스레드 수별 결과가 정리되어 있습니다.
- **분석 txt**: `debug_cases/` 아래에 락프리 문제 발생 시 로그 분석 자료가 있습니다(예: ABA 로그 분석, 2nd CAS 실패 케이스 분석).
- **영상 링크**: 문제별 재현/해결 영상 링크는 `docs/videos/` 아래 텍스트 파일에 참고용으로 정리되어 있습니다.

---

## 5. 주의사항

- 이 레포에는 고의로 크래시나 동시성 문제를 재현하는 테스트가 포함되어 있습니다. 재현 테스트는 정상 종료되지 않을 수 있습니다.
- 재현 테스트와 해결 테스트를 반드시 구분해 실행하세요. 재현 폴더는 문제를 일으키는 코드, 해결 폴더는 이를 막은 코드입니다.
- 동기화 비교 수치는 절대 값보다 비교 조건(스레드 수·시간·계측 방식)이 중요합니다. 조건이 다르면 순위가 바뀔 수 있습니다.
