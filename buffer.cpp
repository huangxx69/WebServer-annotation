/*
 * @Author       : mark
 * @Date         : 2020-06-26
 * @copyleft Apache 2.0
 */ 
#include "buffer.h"

Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}  //读指针和写指针

size_t Buffer::ReadableBytes() const {  //剩余可读缓冲大小
    return writePos_ - readPos_;
}
size_t Buffer::WritableBytes() const {  //可写入缓冲大小（写指针-缓冲区末端）
    return buffer_.size() - writePos_;
}

size_t Buffer::PrependableBytes() const {   //已写已读取区域，相当于空闲区间
    return readPos_;
}

const char* Buffer::Peek() const {  //读指针位置
    return BeginPtr_() + readPos_;
}

void Buffer::Retrieve(size_t len) { //读len长度数据
    assert(len <= ReadableBytes());
    readPos_ += len;
}

void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end );
    Retrieve(end - Peek());
}

void Buffer::RetrieveAll() {    //读可读区域所有数据
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}

std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

void Buffer::HasWritten(size_t len) {
    writePos_ += len;
} 

void Buffer::Append(const std::string& str) {
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);
}
//实际调用的Append。通过EnsureWriteable()函数保证有足够写入空间，然后把未读数据复制到缓冲区前端，移动写指针。
void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWriteable(len);
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}
//如果可写空间（写指针到末尾）不够，调用MakeSpace_()函数扩容
void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len) {
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}

ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    char buff[65535];       //预分配一个临时缓冲区大小为65535字节
    struct iovec iov[2];    //通过iovecs结构体表示待写缓冲区
    const size_t writable = WritableBytes();
    /* 分散读， 保证数据全部读完 */
    iov[0].iov_base = BeginPtr_() + writePos_;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);
    //可以读到固定缓冲区可写区域和整个临时缓冲区中
    const ssize_t len = readv(fd, iov, 2);
    if(len < 0) {
        *saveErrno = errno;
    }
    else if(static_cast<size_t>(len) <= writable) { //如果可写区域够读，把读取数据写进固定缓冲区
        writePos_ += len;
    }
    else {  //如果可写区域不够大，把读取数据写满可写区域，并使用Append()函数读取剩余到临时缓冲区
        writePos_ = buffer_.size();
        Append(buff, len - writable);
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *saveErrno = errno;
        return len;
    } 
    readPos_ += len;
    return len;
}

char* Buffer::BeginPtr_() {
    return &*buffer_.begin();
}

const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}
//如果已读区域+可写区域大小足够，把未读数据移动到缓冲区前端
//如果大小不够，对固定缓冲区进行扩容，使写数据后写指针刚好到缓冲区尾部（尾部扩容到写指针+len+1的地方）
void Buffer::MakeSpace_(size_t len) {
    if(WritableBytes() + PrependableBytes() < len) {
        buffer_.resize(writePos_ + len + 1);
    } 
    else {
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readPos_ + readable;
        assert(readable == ReadableBytes());
    }
}