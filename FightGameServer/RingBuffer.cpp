#include "RingBuffer.h"
#include <iostream>

//기본사이즈는 10KB
RingBuffer::RingBuffer() : _size(1024 * 10)
{
	this->_begin = new char[_size];
	this->_end = this->_begin + _size;
	this->_writePtr = _begin;
	this->_readPtr = _begin;
}

RingBuffer::RingBuffer(int size) : _size(size)
{
	this->_begin = new char[_size];
	this->_end = this->_begin + _size;
	this->_writePtr = _begin;
	this->_readPtr = _begin;
}

RingBuffer::~RingBuffer()
{
	delete[] this->_begin;
}

//최대 사이즈
int RingBuffer::GetBufferSize() const
{
	return this->_end - this->_begin - 1;
}

//버퍼 사이즈 조정
void RingBuffer::Resize(int newSize)
{
	// 새 버퍼 메모리 할당
	char* newBuffer = new char[newSize + 1];
	int usedSize = this->GetUseSize();

	// 래핑된 데이터 복사
	if (this->_writePtr >= this->_readPtr)
	{
		// 데이터가 연속된 경우
		memcpy_s(newBuffer, newSize + 1, this->_readPtr, usedSize);
	}
	else
	{
		// 데이터가 래핑된 경우
		int firstPartSize = this->_end - this->_readPtr; // 끝까지의 크기
		memcpy_s(newBuffer, newSize + 1, this->_readPtr, firstPartSize);
		memcpy_s(newBuffer + firstPartSize, newSize + 1 - firstPartSize, this->_begin, usedSize - firstPartSize);
	}

	// 기존 버퍼 메모리 해제
	delete[] this->_begin;

	// 새 버퍼로 교체
	this->_begin = newBuffer;
	this->_size = newSize + 1;
	this->_end = this->_begin + this->_size;

	// 읽기 및 쓰기 포인터 재설정
	this->_readPtr = this->_begin;            // 새 버퍼의 시작 위치로 이동
	this->_writePtr = this->_begin + usedSize; // 기존 데이터 크기만큼 이동
}


/////////////////////////////////////////////////////////////////////////
// 현재 사용중인 용량 얻기.
//
// Parameters: 없음.
// Return: (int)사용중인 용량.
/////////////////////////////////////////////////////////////////////////	
int RingBuffer::GetUseSize()
{
	if (this->_writePtr >= this->_readPtr)
		return this->_writePtr - this->_readPtr;
	return (this->_writePtr - this->_begin) + (this->_end - this->_readPtr);
}

/////////////////////////////////////////////////////////////////////////
// 현재 버퍼에 남은 용량 얻기. 
//
// Parameters: 없음.
// Return: (int)남은용량.
/////////////////////////////////////////////////////////////////////////
int RingBuffer::GetFreeSize()
{
	if (this->_writePtr >= this->_readPtr)
		return (this->_end - this->_writePtr) + (this->_readPtr - this->_begin - 1);
	return this->_readPtr - this->_writePtr - 1;
}

bool RingBuffer::Enqueue(const char* data, int size)
{
	//남은 공간이 넣을 사이즈보다 작으면 넣지 않을것임
	if (!(this->GetFreeSize() >= size))
		return false;

	//남은 다이렉트 공간에 모두 넣을 수 있으면 한번에 넣어줌
	if (this->DirectEnqueueSize() >= size)
	{
		memcpy_s(this->_writePtr, size, data, size);
		this->MoveWrite(size);
		return true;
	}
	const char* temp = data;
	//두번에 걸쳐서 넣어야 할때
	int fir = this->DirectEnqueueSize();
	memcpy_s(this->_writePtr, fir, temp, fir);
	this->MoveWrite(fir);
	temp += fir;

	int sec = size - fir;
	memcpy_s(this->_writePtr, sec, temp, sec);
	MoveWrite(sec);

	return true;
}

bool RingBuffer::Dequeue(char* dest, int size)
{
	//들어있는 데이터가 받을 데이터보다 작으면 바로 리턴
	if (this->GetUseSize() < size)
		return false;

	//현재 다이렉트 공간이 한번에 뺄 수 있는 크기면 한번에 빼줌
	if (this->DirectDequeueSize() >= size)
	{
		memcpy_s(dest, size, this->_readPtr, size);
		this->MoveRead(size);
		return true;
	}

	char* destTemp = dest;
	//두번에 걸쳐서 빼야 할때
	int fir = this->DirectDequeueSize();
	memcpy_s(destTemp, fir, this->_readPtr, fir);
	this->MoveRead(fir);
	destTemp += fir;

	int sec = size - fir;
	memcpy_s(destTemp, sec, this->_readPtr, sec);
	this->MoveRead(sec);
	return true;
}

bool RingBuffer::Peek(char* dest, int size)
{
	//들어있는 데이터가 받을 데이터보다 작으면 바로 리턴
	if (this->GetUseSize() < size)
		return false;

	//현재 다이렉트 공간이 한번에 뺄 수 있는 크기면 한번에 빼줌
	if (this->DirectDequeueSize() >= size)
	{
		memcpy_s(dest, size, this->_readPtr, size);
		return true;
	}

	//_readPtr 위치 기억
	char* readTemp = this->_readPtr;
	char* destTemp = dest;

	//두번에 걸쳐서 빼야 할때
	int fir = this->DirectDequeueSize();
	memcpy_s(destTemp, fir, this->_readPtr, fir);
	this->MoveRead(fir);
	destTemp += fir;

	int sec = size - fir;
	memcpy_s(destTemp, sec, this->_readPtr, sec);

	//위치 복구
	this->_readPtr = readTemp;

	return true;
}

int RingBuffer::DirectEnqueueSize(void)
{
	if (this->_writePtr >= this->_readPtr)
		return this->_end - this->_writePtr - 1;
	return this->_readPtr - this->_writePtr - 1;
}

int RingBuffer::DirectDequeueSize(void)
{
	if (this->_writePtr >= this->_readPtr)
		return this->_writePtr - this->_readPtr;
	return this->_end - this->_readPtr - 1;
}

int RingBuffer::MoveWrite(int size)
{
	this->_writePtr += size;

	if (this->_writePtr >= this->_end - 1)
	{
		int overflow = this->_writePtr - this->_end;
		this->_writePtr = this->_begin + overflow;
	}
	return size;
}

int RingBuffer::MoveRead(int size)
{
	this->_readPtr += size;

	if (this->_readPtr >= this->_end - 1)
	{
		int overflow = this->_readPtr - this->_end;
		this->_readPtr = this->_begin + overflow;
	}
	return size;
}

char* RingBuffer::GetWriteBufferPtr()
{
	return this->_writePtr;
}

char* RingBuffer::GetReadBufferPtr()
{
	return this->_readPtr;
}

void RingBuffer::ClearBuffer()
{
	this->_writePtr = this->_begin;
	this->_readPtr = this->_begin;
}
