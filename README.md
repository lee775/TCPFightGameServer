# TCP Fight Game Server

`select` 모델 기반의 단일 스레드 TCP 격투 게임 서버 (C++ / Winsock2)

## 소개

2D 격투 게임(TCP Fighter)의 멀티플레이 서버입니다. 클라이언트들이 같은 맵에 접속해 8방향으로 이동하고, 3종류의 공격으로 서로를 타격합니다.

- 이동 시뮬레이션, 공격 판정, 데미지 계산을 서버가 담당하고 결과를 클라이언트에 통보
- 논블로킹 소켓 + `select` 폴링으로 모든 세션의 송수신을 한 스레드에서 처리
- 세션별 링버퍼와 직렬화 버퍼(`Packet`)를 직접 구현해 사용
- 프로토콜 명세 파일 [`PacketDefine.h`](FightGameServer/PacketDefine.h)는 파일 헤더에 출처(gamecodi.com)가 표기된 TCP Fighter 명세를 따름
- 클라이언트는 이 저장소에 포함되어 있지 않음

## 주요 기능

- **접속 처리**: 신규 유저에게 자기 캐릭터 생성 패킷을 보내고, 기존 유저들에게 신규 캐릭터를 알림. 신규 유저에게는 기존 캐릭터 목록과, 이동 중인 캐릭터의 이동 상태까지 전달
- **서버 측 이동 시뮬레이션**: 20ms 고정 틱마다 8방향 이동(X 3px, Y 2px/틱)과 맵 경계 처리를 서버에서 계산
- **이동 좌표 검증**: 이동 정지 패킷의 클라이언트 좌표와 서버 좌표 차이가 `ERROR_RANGE`(50) 이상이면 연결 종료
- **공격 판정**: 공격자 방향(좌/우) 기준 사각 범위로 피격 대상을 찾고 HP 차감 후 `SC_DAMAGE` 통보
- **사망/종료 처리**: HP가 0 이하가 되면 다음 틱에 소켓을 닫고 `SC_DELETE_CHARACTER`를 다른 유저에게 전송

| 공격 | 데미지 | 범위 X | 범위 Y |
|------|-------:|-------:|-------:|
| Attack1 | 2 | 80 | 10 |
| Attack2 | 3 | 90 | 10 |
| Attack3 | 5 | 100 | 20 |

## 아키텍처

### 메인 루프

```
wmain()
 ├─ Initialize()        WSAStartup → socket → bind(0.0.0.0:5000) → listen → 논블로킹 전환
 └─ while (1)
     ├─ netIOProcess()  select(timeout 0)
     │    ├─ listen 소켓 readable → AcceptProc()
     │    ├─ 세션 readable        → RecvProc()    : recv → 수신 링버퍼 → 패킷 조립 → 핸들러 디스패치
     │    └─ 세션 writable        → RingBufSend() : 송신 링버퍼 → send
     └─ Update()        20ms 경과 시 1틱 실행
          ├─ 이동 중인 플레이어 MovePlayer()
          └─ HP ≤ 0 플레이어 Disconnect() → delete → 리스트에서 제거
```

- **단일 스레드**: 네트워크 I/O와 게임 로직이 같은 스레드에서 순차 실행되므로 락이 없음
- **프레임 고정**: `timeBeginPeriod(1)` + `timeGetTime()`으로 경과 시간을 재고, 기준 시각을 20ms씩 누적해 50FPS 로직 틱 유지
- **쓰기 감시 최소화**: 송신 링버퍼에 데이터가 있는 세션만 쓰기 셋(`sset`)에 등록
- **지연 삭제**: 수신 오류나 잘못된 패킷이 오면 즉시 지우지 않고 `_hp = 0`으로 표시만 한 뒤, `Update()`에서 정리해 순회 중 리스트가 바뀌지 않도록 함

### 패킷 흐름

```
[Client] ──CS_MOVE_START──▶ [Server] 서버 좌표로 이동 시작 ──SC_MOVE_START──▶ 다른 클라이언트
[Client] ──CS_MOVE_STOP───▶ [Server] 좌표 오차 검증 → 위치 확정 ──SC_MOVE_STOP──▶ 다른 클라이언트
[Client] ──CS_ATTACKn─────▶ [Server] SC_ATTACKn 전체 전송 → 범위 판정 ──SC_DAMAGE──▶ 전체
```

모든 송신은 즉시 `send()`하지 않고 세션별 송신 링버퍼에 쌓은 뒤, 다음 `select`에서 소켓이 쓰기 가능할 때 한 번에 보냅니다.

## 핵심 구현

### 패킷 조립과 디스패치 — [`FightGameServer.cpp`](FightGameServer/FightGameServer.cpp)

- `RecvProc()`: 링버퍼에 이어 쓸 공간이 충분하면 `GetWriteBufferPtr()`로 `recv`해 복사를 줄이고, 아니면 임시 버퍼로 받아 `Enqueue`
- 헤더 `Peek` → 패킷 코드 `0x89` 확인(불일치 시 연결 종료) → 헤더 + 본문이 다 들어왔을 때만 꺼냄
- 패킷 타입을 키로 하는 `std::unordered_map<BYTE, PacketHandler>` 함수 포인터 테이블(`dispatchTable`)로 핸들러 호출

### 링버퍼 — [`RingBuffer.h`](FightGameServer/RingBuffer.h) / [`RingBuffer.cpp`](FightGameServer/RingBuffer.cpp)

- 기본 10KB, 읽기/쓰기 포인터 방식(한 칸을 비워 가득 참과 빔을 구분)
- `Enqueue` / `Dequeue` / `Peek`: 버퍼 끝에서 감기는 경우 두 번에 나눠 복사
- `DirectEnqueueSize()` / `DirectDequeueSize()` / `MoveWrite()` / `MoveRead()`: 소켓 함수가 버퍼에 직접 읽고 쓸 수 있도록 끊기지 않는 구간 크기 제공
- `Resize()`: 감긴 데이터를 순서대로 새 버퍼에 옮겨 크기 확장

### 직렬화 버퍼 — [`SerialBuffer.h`](FightGameServer/SerialBuffer.h) / [`SerialBuffer.cpp`](FightGameServer/SerialBuffer.cpp)

- `Packet` 클래스: 기본 타입별 `operator<<` / `operator>>` 오버로딩
- 기본은 비트 시프트로 바이트 단위 기록(리틀 엔디언), `USE_MEMCPY` 정의 시 `memcpy` 방식으로 전환
- 공간이 모자라면 `Resize()`로 1000바이트씩 확장, 읽을 데이터가 모자라면 예외 발생
- `GetBuffer()`로 직렬화 결과를 송신 링버퍼에 바로 복사
- 현재 `SC_CREATE_MY_CHARACTER`만 직렬화 버퍼를 쓰고, 나머지 패킷은 `#pragma pack(1)` 구조체를 그대로 복사하는 방식(두 방식의 `mp*` 생성 함수가 함께 있음)

### 서버 측 이동/판정 — [`FightGameServer.cpp`](FightGameServer/FightGameServer.cpp)

- `MovePlayer()`: 8방향별 좌표 증감, `RANGE_MOVE_*` 경계를 넘으면 좌표를 경계값에 고정하고 다른 축 이동은 취소
- `MoveStop()`: 클라이언트가 보낸 정지 좌표를 서버 좌표와 비교해 오차가 크면 연결 종료, 정상이면 위치 확정 후 브로드캐스트
- `CheckAttackCollision()`: 공격자가 바라보는 방향(LL/RR) 앞쪽 사각 범위 안의 대상을 판정

### 프로파일러 — [`Profiling.h`](FightGameServer/Profiling.h) / [`Profiling.cpp`](FightGameServer/Profiling.cpp)

- `PRO_BEGIN` / `PRO_END` 매크로, `QueryPerformanceCounter`로 마이크로초 단위 측정
- 태그별 호출 횟수, 최소/최대, 상·하위 2개씩 뺀 평균을 `profile.txt`로 출력
- 프로젝트에 포함되어 있지만 현재 서버 코드에서 호출하지는 않음

## 패킷/프로토콜

헤더 3바이트 + 본문, 모든 구조체는 `#pragma pack(push, 1)`로 정렬 없이 패킹됩니다.

| 필드 | 크기 | 설명 |
|------|-----:|------|
| `_code` | 1 | 패킷 코드, 항상 `0x89` |
| `_size` | 1 | 본문 크기 |
| `_type` | 1 | 패킷 타입 |

| 타입 | 이름 | 방향 | 본문 |
|-----:|------|------|------|
| 0 | `SC_CREATE_MY_CHARACTER` | S→C | ID, 방향, X, Y, HP |
| 1 | `SC_CREATE_OTHER_CHARACTER` | S→C | ID, 방향, X, Y, HP |
| 2 | `SC_DELETE_CHARACTER` | S→C | ID |
| 10 | `CS_MOVE_START` | C→S | 방향(8방향), X, Y |
| 11 | `SC_MOVE_START` | S→C | ID, 방향, X, Y |
| 12 | `CS_MOVE_STOP` | C→S | 방향(좌/우), X, Y |
| 13 | `SC_MOVE_STOP` | S→C | ID, 방향, X, Y |
| 20 / 22 / 24 | `CS_ATTACK1~3` | C→S | 방향, X, Y |
| 21 / 23 / 25 | `SC_ATTACK1~3` | S→C | ID, 방향, X, Y |
| 30 | `SC_DAMAGE` | S→C | 공격자 ID, 피격자 ID, 피격자 남은 HP |

`CS_SYNC`(250) / `SC_SYNC`(251)는 정의만 있고 서버에서 처리하지 않습니다.

## 기술 스택

- C++ (MSVC, Visual Studio 2022 / 플랫폼 도구 집합 `v143`)
- Win32, Winsock2 (`ws2_32`), 논블로킹 소켓 + `select`
- `winmm` (`timeBeginPeriod`, `timeGetTime`)
- x64 콘솔 애플리케이션, 유니코드 문자 집합

## 프로젝트 구조

```
FightGameServer.sln
FightGameServer/
├─ FightGameServer.cpp   메인 루프, 네트워크 처리, 패킷 핸들러, 이동/공격 로직
├─ PacketDefine.h        패킷 헤더/구조체/타입, 이동 범위·공격 수치 상수
├─ RingBuffer.h/.cpp     세션 송수신용 링버퍼
├─ SerialBuffer.h/.cpp   직렬화 버퍼(Packet)
└─ Profiling.h/.cpp      QPC 기반 구간 프로파일러
```

## 빌드 및 실행

1. Visual Studio 2022에서 `FightGameServer.sln` 열기
2. 구성 `Debug` 또는 `Release`, 플랫폼 `x64` 선택 후 빌드
3. 실행하면 `0.0.0.0:5000`(`SERVERPORT`)에서 접속 대기
4. 같은 프로토콜(`PacketDefine.h`)을 쓰는 TCP Fighter 클라이언트로 접속 (클라이언트는 미포함)

## 한계 및 개선 과제

- `select`를 한 번만 호출하므로 `FD_SETSIZE`(기본 64)를 넘는 소켓은 처리하지 못함
- 공간 분할(섹터) 없이 모든 브로드캐스트를 접속자 전체에게 보냄
- `select` 타임아웃이 0이고 대기가 없어 루프가 쉬지 않고 돎
- 직렬화 버퍼 적용이 일부 패킷에만 되어 있음

## 개발 기간

- 2025-01-02 단일 커밋으로 업로드 (그 이전의 작업 이력은 저장소에 없음)
- 개인 프로젝트
