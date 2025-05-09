#pragma once
// 类似于 ifdef ifndef一样，防止头文件被包含（这种是语言级别的）。这种写法更加的轻量级，是编译器级别的，

/**
* noncopyable被继承以后，派生类对象可以正常构造和析构，但是
* 派生类对象无法进行拷贝和赋值的操作
*/
class noncopyable
{
public:
    noncopyable(const noncopyable& ) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
protected:
    noncopyable() = default;
    ~noncopyable() = default;
};