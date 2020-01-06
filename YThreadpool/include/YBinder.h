#pragma once

#include <tuple>
using namespace std;

class YInvoker
{
	template <bool isMemeberFunctionPointer>
	class _Invoker;

	template <> class _Invoker<true>
	{
		template <typename T>
		static T& _ObjReference(T&& obj) noexcept
		{
			return obj;
		}
		template <typename T>
		static T& _ObjReference(T*& obj) noexcept
		{
			return *obj;
		}
		template <typename T>
		static T& _ObjReference(T*&& obj) noexcept
		{
			return *obj;
		}
	public:
		template <typename Func, typename Obj, typename... Args>
		static auto Invoke(Func func, Obj&& obj, Args&& ... args)
			-> decltype((_ObjReference(declval<Obj>()).*declval<Func>())(declval<Args>()...))
		{
			return (_ObjReference(forward<Obj>(obj)).*func)(forward<Args>(args)...);
		}
	};

	template<> class _Invoker<false>
	{
	public:
		template <typename Func, typename... Args>
		static auto Invoke(Func&& func, Args&& ... args)
			->decltype(declval<Func>()(declval<Args>()...))
		{
			return func(forward<Args>(args)...);
		}
	};
	
	template <typename Func>
	using MyInvoker = _Invoker<is_member_function_pointer_v<decay_t<Func>>>;
public:
	template <typename Func, typename... Args>
	using ResultOf = decltype(MyInvoker<Func>::Invoke(declval<Func>(), declval<Args>()...));

	template <typename Func, typename... Args>
	static auto Invoke(Func&& func, Args&&... args) -> ResultOf<Func, Args...>
	{
		return MyInvoker<Func>::Invoke(forward<Func>(func), forward<Args>(args)...);
	}
};

/*!
 * @class YBinder : 可调用对象(函数指针、函数引用、成员函数指针、函数对象、Lambda表达式)的包装器.
*/
template <typename Func, typename... BoundArgs>
class YBinder
{
	using MyType = YBinder<Func, BoundArgs...>;
	using MyFunc = decay_t<Func>;
	using MyBoundArgs = tuple<decay_t<BoundArgs>...>;
	using MyBoundArgsIndexSequence = index_sequence_for<BoundArgs...>;
public:
	/*!
	 * @typedef ResultOf : 可调用对象的返回类型
	 * @UnboundArgs : 未绑定参数
	*/
	template <typename... UnboundArgs>
	using ResultOf = YInvoker::ResultOf<MyFunc, decay_t<BoundArgs>&..., UnboundArgs...>;

	/*!
	 * @function YBinder : 构造函数
	 * @param func	: 可调用对象.
	 * @param args	: 传递给func位于参数列表前面的0个或多个参数.
	*/
	YBinder(Func&& func, BoundArgs&& ... args) :
		m_func(forward<Func>(func)),
		m_boundArgs(forward<BoundArgs>(args)...)
	{
	}

	/*!
	 * @function operator()	: 调用绑定的可调用对象
	 * @param args	: 传递给可调用对象的位于绑定参数之后的0个或多个参数.
	*/
	template <typename... UnboundArgs>
	auto operator()(UnboundArgs&& ... args) -> ResultOf<UnboundArgs...>
	{
		return _Call(MyBoundArgsIndexSequence(), forward<UnboundArgs>(args)...);
	}
private:
	template <size_t... Idxs, typename... UnboundArgs>
	auto _Call(index_sequence<Idxs...>, UnboundArgs&& ... args)
	{
		return YInvoker::Invoke(m_func, get<Idxs>(m_boundArgs)..., forward<UnboundArgs>(args)...);
	}

	MyFunc		m_func;
	MyBoundArgs	m_boundArgs;
};

template <typename Func, typename... BoundArgs>
auto YBinderMake(Func&& func, BoundArgs&& ... args)
{
	return YBinder<Func, BoundArgs...>(forward<Func>(func), forward<BoundArgs>(args)...);
}

template <typename Func, typename... BoundArgs>
auto YBinderNew(Func&& func, BoundArgs&& ... args)
{
	return new YBinder<Func, BoundArgs...>(forward<Func>(func), forward<BoundArgs>(args)...);
}
