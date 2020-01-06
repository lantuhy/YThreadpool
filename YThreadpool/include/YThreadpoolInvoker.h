#pragma once

#include "YBinder.h"

template <typename Func, typename... BoundArgs>
class YThreadpoolInvoker
{
	using MyBinder = YBinder<Func, BoundArgs...>;
public:
	template <typename... UnboundArgs>
	using ResultOf = typename MyBinder::template ResultOf<UnboundArgs...>;

	YThreadpoolInvoker(Func&& func, BoundArgs&& ... args) :
		m_binder(forward<Func>(func), forward<BoundArgs>(args)...)
	{
	}

	template <typename... UnboundArgs>
	auto Invoke(UnboundArgs&& ... args) -> ResultOf<UnboundArgs...>
	{
		return m_binder(forward<UnboundArgs>(args)...);
	}
private:
	MyBinder m_binder;
};

