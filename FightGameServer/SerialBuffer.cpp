#include "SerialBuffer.h"
#include <iostream>
#include <time.h>

//#define USE_MEMCPY	1

Packet::Packet() : _bufferSize(BUFFER_DEFAULT), _dataSize(0), _writeIdx(0), _readIdx(0), _resizeCount(0)
{
	this->_buffer = new char[this->_bufferSize];
	this->_end = this->_buffer + this->_bufferSize;
	this->_writePtr = this->_buffer;
	this->_readPtr = this->_buffer;
}

Packet::Packet(int bufferSize) : _bufferSize(bufferSize), _dataSize(0), _writeIdx(0), _readIdx(0)
{
	this->_buffer = new char[this->_bufferSize];
	this->_end = this->_buffer + this->_bufferSize;
	this->_writePtr = this->_buffer;
	this->_readPtr = this->_buffer;
}

Packet::~Packet()
{
	delete[] this->_buffer;
}

void Packet::Resize()
{
	this->_resizeCount++;

	//기본크기만큼 더 늘려줌
	char* tempBuf = new char[this->_bufferSize + BUFFER_DEFAULT];
	memcpy_s(tempBuf, this->_bufferSize, this->_buffer, this->_bufferSize);

	this->_writePtr = tempBuf + (this->_writePtr - this->_buffer);
	this->_readPtr = tempBuf + (this->_readPtr - this->_buffer);

	this->_bufferSize = this->_bufferSize + BUFFER_DEFAULT;
	this->_end = tempBuf + this->_bufferSize;

	delete[] this->_buffer;
	this->_buffer = tempBuf;
	wprintf(L"re : %d\n", this->_resizeCount);
	this->LogPrint();
}

void Packet::Clear(void)
{
	this->_writePtr = this->_buffer;
	this->_readPtr = this->_buffer;
	this->_writeIdx = 0;
	this->_readIdx = 0;
	this->_dataSize = 0;
}

int Packet::GetFreeSize()
{
	return this->_bufferSize - this->_dataSize;
}

int Packet::MoveWritePos(int size)
{
	this->_writePtr += size;
	this->_writeIdx += size;
	this->_dataSize += size;

	if (this->_writePtr >= this->_end - 1)
	{
		return -1;
	}
	return 0;
}

int Packet::MoveReadPos(int size)
{
	this->_readPtr += size;
	this->_readIdx += size;
	this->_dataSize -= size;

	if (this->_readPtr >= this->_end - 1)
	{
		return -1;
	}
	return 0;
}

Packet& Packet::operator=(Packet& packet)
{
	if (this != &packet)
	{
		delete[] _buffer;

		this->_bufferSize = packet._bufferSize;
		this->_dataSize = packet._dataSize;
		this->_writeIdx = packet._writeIdx;
		this->_readIdx = packet._readIdx;
		this->_resizeCount = packet._resizeCount;

		this->_buffer = new char[this->_bufferSize];
		memcpy_s(this->_buffer, this->_bufferSize, packet.GetBufferPtr(), this->_bufferSize);

		_writePtr = _buffer + (packet._writePtr - packet._buffer);
		_readPtr = _buffer + (packet._readPtr - packet._buffer);
		_end = _buffer + _bufferSize;

	}
	return *this;
}

Packet& Packet::operator<<(unsigned char value)
{
	//여유공간이 없으면 리사이즈
	if (this->GetFreeSize() < sizeof(value))
		this->Resize();

#if USE_MEMCPY
	memcpy_s(this->_writePtr, sizeof(value), (char*)&value, sizeof(value));
	MoveWritePos(sizeof(value));
#else
	//for (int i = 0; i < sizeof(value); i++)
	//{
	//	this->_buffer[this->_writeIdx++] = value >> (i * 8);
	//	this->_dataSize++;
	//	this->_writePtr++;
	//}
	this->_buffer[this->_writeIdx++] = value;
	++this->_dataSize;
	++this->_writePtr;
#endif
	return *this;
}

Packet& Packet::operator<<(char value)
{
	//여유공간이 없으면 리사이즈
	if (this->GetFreeSize() < sizeof(value))
		this->Resize();

#if USE_MEMCPY
	memcpy_s(this->_writePtr, sizeof(value), (char*)&value, sizeof(value));
	MoveWritePos(sizeof(value));
#else
	this->_buffer[this->_writeIdx++] = value;
	++this->_dataSize;
	++this->_writePtr;
#endif
	return *this;
}

Packet& Packet::operator<<(short value)
{
	//여유공간이 없으면 리사이즈
	if (this->GetFreeSize() < sizeof(value))
		this->Resize();

#if USE_MEMCPY
	memcpy_s(this->_writePtr, sizeof(value), (char*)&value, sizeof(value));
	MoveWritePos(sizeof(value));
#else
	this->_buffer[this->_writeIdx++] = value;
	this->_buffer[this->_writeIdx++] = value >> 8;
	this->_dataSize += sizeof(value);
	this->_writePtr += sizeof(value);
#endif
	return *this;
}

Packet& Packet::operator<<(unsigned short value)
{
	//여유공간이 없으면 리사이즈
	if (this->GetFreeSize() < sizeof(value))
		this->Resize();

#if USE_MEMCPY
	memcpy_s(this->_writePtr, sizeof(value), (char*)&value, sizeof(value));
	MoveWritePos(sizeof(value));
#else
	this->_buffer[this->_writeIdx++] = value;
	this->_buffer[this->_writeIdx++] = value >> 8;
	this->_dataSize += sizeof(value);
	this->_writePtr += sizeof(value);
#endif
	return *this;
}

//int<<
Packet& Packet::operator<<(int value)
{
	//여유공간이 없으면 리사이즈
	if (this->GetFreeSize() < sizeof(value))
		this->Resize();

#if USE_MEMCPY
	memcpy_s(this->_writePtr, sizeof(value), (char*)&value, sizeof(value));
	MoveWritePos(sizeof(value));
#else
	this->_buffer[this->_writeIdx++] = value;
	this->_buffer[this->_writeIdx++] = value >> 8;
	this->_buffer[this->_writeIdx++] = value >> 16;
	this->_buffer[this->_writeIdx++] = value >> 24;
	this->_dataSize += sizeof(value);
	this->_writePtr += sizeof(value);
#endif
	return *this;
}

Packet& Packet::operator<<(DWORD value)
{
	//여유공간이 없으면 리사이즈
	if (this->GetFreeSize() < sizeof(value))
		this->Resize();

#if USE_MEMCPY
	memcpy_s(this->_writePtr, sizeof(value), (char*)&value, sizeof(value));
	MoveWritePos(sizeof(value));
#else
	this->_buffer[this->_writeIdx++] = value;
	this->_buffer[this->_writeIdx++] = value >> 8;
	this->_buffer[this->_writeIdx++] = value >> 16;
	this->_buffer[this->_writeIdx++] = value >> 24;
	this->_dataSize += sizeof(value);
	this->_writePtr += sizeof(value);
#endif
	return *this;
}

Packet& Packet::operator<<(long value)
{
	//여유공간이 없으면 리사이즈
	if (this->GetFreeSize() < sizeof(value))
		this->Resize();

#if USE_MEMCPY
	memcpy_s(this->_writePtr, sizeof(value), (char*)&value, sizeof(value));
	MoveWritePos(sizeof(value));
#else
	this->_buffer[this->_writeIdx++] = value;
	this->_buffer[this->_writeIdx++] = value >> 8;
	this->_buffer[this->_writeIdx++] = value >> 16;
	this->_buffer[this->_writeIdx++] = value >> 24;
	this->_dataSize += sizeof(value);
	this->_writePtr += sizeof(value);
#endif
	return *this;
}

Packet& Packet::operator<<(float value)
{
	//여유공간이 없으면 리사이즈
	if (this->GetFreeSize() < sizeof(value))
		this->Resize();

	memcpy_s(this->_writePtr, sizeof(value), (char*)&value, sizeof(value));
	MoveWritePos(sizeof(value));
	return *this;
}

Packet& Packet::operator<<(__int64 value)
{
	//여유공간이 없으면 리사이즈
	if (this->GetFreeSize() < sizeof(value))
		this->Resize();

#if USE_MEMCPY
	memcpy_s(this->_writePtr, sizeof(value), (char*)&value, sizeof(value));
	MoveWritePos(sizeof(value));
#else
	this->_buffer[this->_writeIdx++] = value;
	this->_buffer[this->_writeIdx++] = value >> 8;
	this->_buffer[this->_writeIdx++] = value >> 16;
	this->_buffer[this->_writeIdx++] = value >> 24;
	this->_buffer[this->_writeIdx++] = value >> 32;
	this->_buffer[this->_writeIdx++] = value >> 40;
	this->_buffer[this->_writeIdx++] = value >> 48;
	this->_buffer[this->_writeIdx++] = value >> 56;
	this->_dataSize += sizeof(value);
	this->_writePtr += sizeof(value);
#endif
	return *this;
}

Packet& Packet::operator<<(double value)
{
	//여유공간이 없으면 리사이즈
	if (this->GetFreeSize() < sizeof(value))
		this->Resize();

	memcpy_s(this->_writePtr, sizeof(value), (char*)&value, sizeof(value));
	MoveWritePos(sizeof(value));
	return *this;
}

Packet& Packet::operator>>(BYTE& value)
{
	//해당 크기만큼 남아있지 않으면 그냥 리턴
	if (this->GetDataSize() < sizeof(value))
		throw "Data not available.";

#if USE_MEMCPY
	memcpy_s((char*)&value, sizeof(value), this->_readPtr, sizeof(value));
	MoveReadPos(sizeof(value));
#else
	value = 0;
	//for (int i = 0; i < sizeof(value); i++)
	//{
	//	value += (*(this->_buffer + this->_readIdx + i) << (i * 8));
	//}
	value += *(this->_buffer + this->_readIdx);
	this->_readIdx += sizeof(value);
	this->_dataSize -= sizeof(value);
	this->_readPtr += sizeof(value);

#endif
	if (this->_readIdx == this->_writeIdx)
		this->Clear();
	return *this;
}

Packet& Packet::operator>>(char& value)
{
	//해당 크기만큼 남아있지 않으면 그냥 리턴
	if (this->GetDataSize() < sizeof(value))
		throw "Data not available.";

#if USE_MEMCPY
	memcpy_s((char*)&value, sizeof(value), this->_readPtr, sizeof(value));
	MoveReadPos(sizeof(value));
#else
	value = 0;
	value += *(this->_buffer + this->_readIdx);
	this->_readIdx += sizeof(value);
	this->_dataSize -= sizeof(value);
	this->_readPtr += sizeof(value);
#endif
	if (this->_readIdx == this->_writeIdx)
		this->Clear();
	return *this;
}

Packet& Packet::operator>>(short& value)
{
	//해당 크기만큼 남아있지 않으면 그냥 리턴
	if (this->GetDataSize() < sizeof(value))
		throw "Data not available.";

#if USE_MEMCPY
	memcpy_s((char*)&value, sizeof(value), this->_readPtr, sizeof(value));
	MoveReadPos(sizeof(value));
#else
	value = 0;
	value += *(this->_buffer + this->_readIdx);
	value += *(this->_buffer + this->_readIdx + 1) << 8;
	this->_readIdx += sizeof(value);
	this->_dataSize -= sizeof(value);
	this->_readPtr += sizeof(value);
#endif
	if (this->_readIdx == this->_writeIdx)
		this->Clear();
	return *this;
}

Packet& Packet::operator>>(WORD& value)
{
	//해당 크기만큼 남아있지 않으면 그냥 리턴
	if (this->GetDataSize() < sizeof(value))
		throw "Data not available.";

#if USE_MEMCPY
	memcpy_s((char*)&value, sizeof(value), this->_readPtr, sizeof(value));
	MoveReadPos(sizeof(value));
#else
	value = 0;
	value += *(this->_buffer + this->_readIdx);
	value += *(this->_buffer + this->_readIdx + 1) << 8;
	this->_readIdx += sizeof(value);
	this->_dataSize -= sizeof(value);
	this->_readPtr += sizeof(value);
#endif
	if (this->_readIdx == this->_writeIdx)
		this->Clear();
	return *this;
}

//int>>
Packet& Packet::operator>>(int& value)
{
	//해당 크기만큼 남아있지 않으면 그냥 리턴
	if (this->GetDataSize() < sizeof(value))
		throw "Data not available.";

#if USE_MEMCPY
	memcpy_s((char*)&value, sizeof(value), this->_readPtr, sizeof(value));
	MoveReadPos(sizeof(value));
#else
	value = 0;
	value += *(this->_buffer + this->_readIdx);
	value += *(this->_buffer + this->_readIdx + 1) << 8;
	value += *(this->_buffer + this->_readIdx + 2) << 16;
	value += *(this->_buffer + this->_readIdx + 3) << 24;
	this->_readIdx += sizeof(value);
	this->_dataSize -= sizeof(value);
	this->_readPtr += sizeof(value);
#endif
	if (this->_readIdx == this->_writeIdx)
		this->Clear();
	return *this;
}

Packet& Packet::operator>>(DWORD& value)
{
	//해당 크기만큼 남아있지 않으면 그냥 리턴
	if (this->GetDataSize() < sizeof(value))
		throw "Data not available.";

#if USE_MEMCPY
	memcpy_s((char*)&value, sizeof(value), this->_readPtr, sizeof(value));
	MoveReadPos(sizeof(value));
#else
	value = 0;
	value += *(this->_buffer + this->_readIdx);
	value += *(this->_buffer + this->_readIdx + 1) << 8;
	value += *(this->_buffer + this->_readIdx + 2) << 16;
	value += *(this->_buffer + this->_readIdx + 3) << 24;
	this->_readIdx += sizeof(value);
	this->_dataSize -= sizeof(value);
	this->_readPtr += sizeof(value);
#endif
	if (this->_readIdx == this->_writeIdx)
		this->Clear();
	return *this;
}

Packet& Packet::operator>>(float& value)
{
	//해당 크기만큼 남아있지 않으면 그냥 리턴
	if (this->GetDataSize() < sizeof(value))
		throw "Data not available.";

	memcpy_s((char*)&value, sizeof(value), this->_readPtr, sizeof(value));
	MoveReadPos(sizeof(value));
	if (this->_readIdx == this->_writeIdx)
		this->Clear();
	return *this;
}

Packet& Packet::operator>>(__int64& value)
{
	//해당 크기만큼 남아있지 않으면 그냥 리턴
	if (this->GetDataSize() < sizeof(value))
		throw "Data not available.";

#if USE_MEMCPY
	memcpy_s((char*)&value, sizeof(value), this->_readPtr, sizeof(value));
	MoveReadPos(sizeof(value));
#else
	value = 0;
	value += *(this->_buffer + this->_readIdx);
	value += *(this->_buffer + this->_readIdx + 1) << 8;
	value += *(this->_buffer + this->_readIdx + 2) << 16;
	value += *(this->_buffer + this->_readIdx + 3) << 24;
	value += *(this->_buffer + this->_readIdx + 4) << 32;
	value += *(this->_buffer + this->_readIdx + 5) << 40;
	value += *(this->_buffer + this->_readIdx + 6) << 48;
	value += *(this->_buffer + this->_readIdx + 7) << 56;
	this->_readIdx += sizeof(value);
	this->_dataSize -= sizeof(value);
	this->_readPtr += sizeof(value);
#endif
	if (this->_readIdx == this->_writeIdx)
		this->Clear();
	return *this;
}

Packet& Packet::operator>>(double& value)
{
	//해당 크기만큼 남아있지 않으면 그냥 리턴
	if (this->GetDataSize() < sizeof(value))
		throw "Data not available.";

	memcpy_s((char*)&value, sizeof(value), this->_readPtr, sizeof(value));
	MoveReadPos(sizeof(value));

	if (this->_readIdx == this->_writeIdx)
		this->Clear();
	return *this;
}

int Packet::GetData(char* dest, int size)
{
	if (this->GetDataSize() < size)
		throw "Data not available.";

#if USE_MEMCPY
	memcpy_s(dest, size, this->_readPtr, size);
	MoveReadPos(size);
#else
	for (int i = 0; i < size; i++)
	{
		dest[i] = *(this->_readPtr + i);
	}
	this->_readIdx += size;
	this->_dataSize -= size;
	this->_readPtr += size;
#endif
	if (this->_readIdx == this->_writeIdx)
		this->Clear();
	return size;
}

int Packet::PutData(char* src, int srcSize)
{
	//여유공간이 없으면 리사이즈
	if (this->GetFreeSize() < srcSize)
		this->Resize();

#if USE_MEMCPY
	memcpy_s(this->_writePtr, srcSize, src, srcSize);
	MoveWritePos(srcSize);
#else
	for (int i = 0; i < srcSize; i++)
	{
		this->_buffer[this->_writeIdx++] = src[i];
	}
	this->_dataSize += srcSize;
	this->_writePtr += srcSize;
#endif
	return srcSize;
}

int Packet::GetBuffer(char* dest, int destSize)
{
	int res;
	if (this->_dataSize > destSize)
	{
		memcpy_s(dest, destSize, this->_readPtr, destSize);
		res = destSize;
	}
	else
	{
		memcpy_s(dest, this->_dataSize, this->_readPtr, this->_dataSize);
		res = this->_dataSize;
	}
	MoveReadPos(res);
	if (this->_readIdx == this->_writeIdx)
		this->Clear();
	return res;
}

void Packet::LogPrint()
{
	FILE* file;
	_wfopen_s(&file, L"Log/log.txt", L"wt");

	fwprintf(file, L"mode : %d\n", this->_bufOption);
	for (__int64 i = 0; i < this->_bufferSize; ++i) {
		// 한 줄에 16바이트씩 출력
		if (i % 16 == 0) {
			fwprintf(file, L"\n%08zx  ", i); // 바이트 오프셋 표시
		}
		fwprintf(file, L"%02x ", this->_buffer[i]); // 각 바이트를 16진수로 출력
	}
	fwprintf(file, L"\n");

	fclose(file);
}
