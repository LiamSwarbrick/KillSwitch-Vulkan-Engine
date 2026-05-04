#ifndef CORE_EVENTS_EVENT_H
#define CORE_EVENTS_EVENT_H

#include <functional>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Forward declaring Subscription class so we don't have cyclic includes
template<typename ...Args> class Subscription;

template<typename ...Args>
class Event
{
public:
	using Callback = std::function<void(Args...)>;
	using SubscriptionID = uint32_t;
	using SubscriptionType = Subscription<Args...>;

	static constexpr SubscriptionID InvalidID = UINT32_MAX;

	SubscriptionType Subscribe(Callback callback)
	{
		SubscriptionID id = m_nextID++;
		if (m_invoking)
			m_pendingAdds.push_back({ id, std::move(callback) });
		else
			m_subscribers[id] = std::move(callback);

		return SubscriptionType(*this, id);
	}

	// Delayed unsubscription
	void Unsubscribe(SubscriptionID id)
	{
		if (m_invoking)
			m_pendingRemovals.push_back(id);
		else
			m_subscribers.erase(id);
	}

	SubscriptionType operator+=(Callback callback)
	{
		return Subscribe(callback);
	}

	void operator-=(SubscriptionID id)
	{
		Unsubscribe(id);
	}

	void Invoke(Args... args)
	{
		m_invoking = true;

		for (auto& [id, callback] : m_subscribers)
		{
			callback(args...);
		}

		m_invoking = false;

		for (SubscriptionID id : m_pendingRemovals)
			m_subscribers.erase(id);
		m_pendingRemovals.clear();

		for (auto& [id, callback] : m_pendingAdds)
			m_subscribers[id] = std::move(callback);
		m_pendingAdds.clear();
	}

	int GetSubscriptorCount() const { return m_subscribers.size(); }
	void Clear() { m_subscribers.clear(); }

private:


private:
	std::unordered_map<SubscriptionID, Callback> m_subscribers;
	std::vector<std::pair<SubscriptionID, Callback>> m_pendingAdds;
	std::vector<SubscriptionID> m_pendingRemovals;
	SubscriptionID m_nextID = 0;
	bool m_invoking = false;
	//int m_invokeDepth = 0;
};

#endif // !CORE_EVENTS_EVENT_H
