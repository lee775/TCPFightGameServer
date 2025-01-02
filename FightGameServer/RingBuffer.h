#pragma once

class RingBuffer
{
private:
	//버퍼의 시작지점
	char* _begin;
	//버퍼의 끝지점
	char* _end;
	//쓰기를 시작하는 버퍼지점
	char* _writePtr;
	//읽기를 시작하는 버퍼지점
	char* _readPtr;
	//사이즈
	int _size;
public:
	RingBuffer();
	RingBuffer(int size);
	~RingBuffer();

	int GetBufferSize() const;

	void Resize(int size);

	int GetUseSize();
	int GetFreeSize();

	bool Enqueue(const char* data, int size);
	bool Dequeue(char* dest, int size);
	bool Peek(char* dest, int size);

	int	DirectEnqueueSize(void);
	int	DirectDequeueSize(void);

	int	MoveWrite(int size);
	int	MoveRead(int size);

	char* GetWriteBufferPtr();
	char* GetReadBufferPtr();

	void ClearBuffer();
};