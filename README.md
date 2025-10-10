# LockFree-DataStructures
Lock-Free stack, queue, memory pool implementations with ABA, page decommit, order inversion issues reproduction &amp; solutions

< 폴더 구조 >
1. debug_cases
   - 2nd CAS 실패, ABA, PageDecommit 발생시 해당 자료구조에 어떤 스레드가 무슨 작업 했는지 로그 및 해석 자료
     
2. docs
   - ppt
   - 영상 링크
   - spinlock, srwlock, lockfree 테스트 결과 및 실행 프로그램
     
3. include
   - 락프리 큐(공용 노드 풀 사용 버전, 안쓰는 버전)
   - 락프리 스택
   - 락프리 메모리 풀

4. issues
   - 락프리 사용 시 발생 가능한 문제 재현 코드 및 해결 코드
   - reproduce 폴더에 해당 솔루션 있음.
    
6. test
   - spinlock, srwlock, lockfree 테스트 프로젝트

< 문제 재현 및 해결 영상 >
1. ABA 문제
   - 원인 : 동일 top 노드의 재활용을 인식 하지 못함.
   - 해결 : top 노드 멤버 변수에 노드 주소값 + tag 값을 설정하여 재활용 확인
   - 재현 영상 링크 : https://youtu.be/tbD-PNxHf5s
   - 해결 영상 링크 : https://youtu.be/LjmOqV8cS9M
     
2. Page Decommit 문제
   - 원인 : 힙에 반납한 노드가 있던 페이지를 힙이 Decommit 시킴
   - 해결 : 메모리 관리 주체를 내가 함.(메모리 풀 사용)
   - 재현 영상 링크 : https://youtu.be/tFWJiXKYj5Y
   - 해결 영상 링크 : https://youtu.be/pd9q8vR92S4
       
3. Queue 순서 뒤바뀜 문제
   - 원인 : 1stCAS는 tail이 가리키는 노드의 next멤버변수가 nullptr인지를 체크하지 그 노드가 실제 tail임을 보장하지 않음.
   - 해결 : Enqueue 작업 시 next 멤버 변수를 nullptr이 아닌 특정 값으로 초기화
   - 재현 영상 링크 : https://youtu.be/gUviydSV0wA
   - 해결 영상 링크 : https://youtu.be/7gOG7qrjBoM
     
4. 공용 노드 풀 사용 문제
   - 원인 : 1stCAS는 tail이 가리키는 next 멤버변수가 nullptr인지 체크하는것이지 그 tail이 기존 큐의 tail인지 다른 큐의 tail인지는 보장하지 않음.
   - 해결 : 1stCAS를 할때 next멤버변수가 nullptr인지 체크 + 기존 큐인지 체크 (Queue ID 삽입)
   - 재현 영상 링크 : https://youtu.be/iPoZmHwS0OM
   - 해결 영상 링크 : https://youtu.be/Jkn-RgcsvVc


< 동기화 방식 성능 비교>
1. ppt 참조
2. 요약
   - 논리 코어 갯수 > 스레드 갯수 : srwlock > spinlock = lockfree
   - 논리 코어 갯수 < 스레드 갯수 : srwlock > lockfree > spinlock
