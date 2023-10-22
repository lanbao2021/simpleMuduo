#pragma once // 防止头文件被重复包含

// 继承noncopyable类，可以防止派生类对象进行拷贝构造和赋值操作
// 派生类为啥不能进行拷贝构造和赋值操作？是因为派生类要先构造基类对象，再构造派生类对象
// 而基类对象不可拷贝构造，所以派生类对象也不可拷贝构造
class noncopyable
{
public:
    noncopyable(const noncopyable &) = delete;            // 禁止外部拷贝构造
    noncopyable &operator=(const noncopyable &) = delete; // 禁止外部赋值操作
protected:
    noncopyable() = default;  // 允许默认构造
    ~noncopyable() = default; // 允许默认析构
};