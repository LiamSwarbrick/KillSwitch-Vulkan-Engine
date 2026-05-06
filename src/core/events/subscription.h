#ifndef CORE_EVENTS_SUBSCRIPTION_H
#define	CORE_EVENTS_SUBSCRIPTION_H

#include "event.h"

template<typename ...Args>
class Subscription
{
public:
	using EventType = Event<Args...>;
	using SubscriptionID = typename EventType::SubscriptionID;

	Subscription() = default;

	Subscription(EventType& event, SubscriptionID id)
		: event(&event), id(id)
	{
	}

	~Subscription() { Unsubscribe(); }

	// Non-copyable
	Subscription(const Subscription&) = delete;
	Subscription& operator=(const Subscription&) = delete;

	// Movable
	Subscription(Subscription&& other) noexcept
		: event(other.event), id(other.id)
	{
		other.event = nullptr;
		other.id = EventType::InvalidID;
	}

	Subscription& operator=(Subscription&& other) noexcept
	{
		Unsubscribe();

		event = other.event;
		id = other.id;
		other.event = nullptr;
		other.id = EventType::InvalidID;

		return *this;
	}

	// Public to be able to manually unsubscribe, but on destruction it unsubscribes anyway
	void Unsubscribe()
	{
		if (event && id != EventType::InvalidID)
		{
			*event -= id; // unsubscribing from the event
			event = nullptr;
			id = EventType::InvalidID;
		}
	}

	bool isValid() const
	{
		return event != nullptr && id != EventType::InvalidID;
	}

private:
	EventType* event = nullptr;
	SubscriptionID id = EventType::InvalidID;
};


#endif // !CORE_EVENTS_SUBSCRIPTION_H

