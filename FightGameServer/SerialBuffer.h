#ifndef  __PACKET__
#define  __PACKET__

#include <Windows.h>

class Packet
{
public:

	/*---------------------------------------------------------------
	Packet Enum.

	----------------------------------------------------------------*/
	enum en_PACKET
	{
		BUFFER_DEFAULT = 1000		// 패킷의 기본 버퍼 사이즈.
	};

	//////////////////////////////////////////////////////////////////////////
	// 생성자, 파괴자.
	//
	// Return:
	//////////////////////////////////////////////////////////////////////////
	Packet();
	Packet(int bufferSize);

	virtual	~Packet();

	//리사이즈
	void Resize();


	//////////////////////////////////////////////////////////////////////////
	// 패킷 청소.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void Clear(void);


	//////////////////////////////////////////////////////////////////////////
	// 버퍼 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int)패킷 버퍼 사이즈 얻기.
	//////////////////////////////////////////////////////////////////////////
	int	GetBufferSize(void) { return _bufferSize; }

	//////////////////////////////////////////////////////////////////////////
	// 현재 사용중인 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int)사용중인 데이타 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int	GetDataSize(void) { return _dataSize; }

	int GetFreeSize();

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 포인터 얻기.
	//
	// Parameters: 없음.
	// Return: (char *)버퍼 포인터.
	//////////////////////////////////////////////////////////////////////////
	char* GetBufferPtr(void) { return _buffer; }

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 Pos 이동. (음수이동은 안됨)
	// GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
	//
	// Parameters: (int) 이동 사이즈.
	// Return: (int) 이동된 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		MoveWritePos(int size);
	int		MoveReadPos(int size);

	/* ============================================================================= */
	// 연산자 오버로딩
	/* ============================================================================= */
	Packet& operator = (Packet& packet);

	//////////////////////////////////////////////////////////////////////////
	// 넣기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	Packet& operator << (unsigned char value);
	Packet& operator << (char value);

	Packet& operator << (short value);
	Packet& operator << (unsigned short value);

	Packet& operator << (int value);
	Packet& operator << (DWORD value);
	Packet& operator << (long value);
	Packet& operator << (float value);

	Packet& operator << (__int64 value);
	Packet& operator << (double value);


	//////////////////////////////////////////////////////////////////////////
	// 빼기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	Packet& operator >> (BYTE& value);
	Packet& operator >> (char& value);

	Packet& operator >> (short& value);
	Packet& operator >> (WORD& value);

	Packet& operator >> (int& value);
	Packet& operator >> (DWORD& value);
	Packet& operator >> (float& value);

	Packet& operator >> (__int64& value);
	Packet& operator >> (double& value);




	//////////////////////////////////////////////////////////////////////////
	// 데이타 얻기.
	//
	// Parameters: (char *)Dest 포인터. (int)Size.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		GetData(char* dest, int size);

	//////////////////////////////////////////////////////////////////////////
	// 데이타 삽입.
	//
	// Parameters: (char *)Src 포인터. (int)SrcSize.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		PutData(char* src, int srcSize);

	//직렬화 버퍼를 통채로 복사
	int GetBuffer(char* dest, int destSize);

	void LogPrint();

protected:
	int	_bufferSize;

	//------------------------------------------------------------
	// 현재 버퍼에 사용중인 사이즈.
	//------------------------------------------------------------
	int	_dataSize;

	char* _buffer;
	char* _end;
	//memcpy를 위한 ptr
	char* _writePtr;
	char* _readPtr;
	//시프트 연산을 위한 index
	int _writeIdx;
	int _readIdx;
	//_resizeCount
	int _resizeCount;

	int _bufOption;
};

#endif