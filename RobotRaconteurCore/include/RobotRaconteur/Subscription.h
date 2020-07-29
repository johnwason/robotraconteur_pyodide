/** 
 * @file Subscription.h
 * 
 * @author John Wason, PhD
 * 
 * @copyright Copyright 2011-2020 Wason Technology, LLC
 *
 * @par License
 * Software License Agreement (Apache License)
 * @par
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * @par
 * http://www.apache.org/licenses/LICENSE-2.0
 * @par
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "RobotRaconteur/Client.h"

#include <boost/tuple/tuple_comparison.hpp>
#include <boost/unordered_set.hpp>

#pragma once

namespace RobotRaconteur
{
	namespace detail
	{
		class Discovery;
		class Discovery_nodestorage;
		class ServiceInfo2Subscription_client;
		class ServiceSubscription_client;
		class ServiceSubscription_retrytimer;
		class WireSubscription_connection;
		class PipeSubscription_connection;
		class WireSubscription_send_iterator;
		class PipeSubscription_send_iterator;
		class ServiceSubscription_custom_member_subscribers;
	}

	class ROBOTRACONTEUR_CORE_API WireConnectionBase;
	template <typename T>
	class WireConnection;
	class ROBOTRACONTEUR_CORE_API WireSubscriptionBase;
	template <typename T>
	class WireSubscription;

	class ROBOTRACONTEUR_CORE_API PipeEndpointBase;
	template <typename T>
	class PipeEndpoint;
	class ROBOTRACONTEUR_CORE_API PipeSubscriptionBase;
	template <typename T>
	class PipeSubscription;

	class ROBOTRACONTEUR_CORE_API ServiceSubscriptionFilterNode
	{
	public:
		::RobotRaconteur::NodeID NodeID;
		std::string NodeName;
		std::string Username;
		RR_INTRUSIVE_PTR<RRMap<std::string, RRValue> > Credentials;
	};

	class ROBOTRACONTEUR_CORE_API ServiceSubscriptionFilter
	{
	public:
		std::vector<RR_SHARED_PTR<ServiceSubscriptionFilterNode> > Nodes;
		std::vector<std::string> ServiceNames;
		std::vector<std::string> TransportSchemes;
		boost::function<bool(const ServiceInfo2&) > Predicate;
		uint32_t MaxConnections;
	};

	class ROBOTRACONTEUR_CORE_API ServiceSubscriptionClientID
	{
	public:

		::RobotRaconteur::NodeID NodeID;
		std::string ServiceName;

		ServiceSubscriptionClientID(const ::RobotRaconteur::NodeID& nodeid, boost::string_ref service_name);

		ServiceSubscriptionClientID();

		bool operator == (const ServiceSubscriptionClientID &id2) const;

		bool operator != (const ServiceSubscriptionClientID &id2) const;

		bool operator <(const ServiceSubscriptionClientID& id2) const;
	};


	class IServiceSubscription
	{
	public:

		friend class detail::Discovery;

	protected:
		virtual void Init(const std::vector<std::string>& service_types, RR_SHARED_PTR<ServiceSubscriptionFilter> filter) = 0;
		virtual void NodeUpdated(RR_SHARED_PTR<detail::Discovery_nodestorage> storage) = 0;
		virtual void NodeLost(RR_SHARED_PTR<detail::Discovery_nodestorage> storage) = 0;

	public:
		virtual void Close() = 0;

	};

	class ROBOTRACONTEUR_CORE_API ServiceInfo2Subscription : public IServiceSubscription, public RR_ENABLE_SHARED_FROM_THIS<ServiceInfo2Subscription>, private boost::noncopyable
	{
	public:

		friend class detail::Discovery;		

		typedef boost::signals2::connection event_connection;

		std::map<ServiceSubscriptionClientID, ServiceInfo2 > GetDetectedServiceInfo2();

		event_connection AddServiceDetectedListener(boost::function<void(RR_SHARED_PTR<ServiceInfo2Subscription>, const ServiceSubscriptionClientID&, const ServiceInfo2&)> handler);
		event_connection AddServiceLostListener(boost::function<void(RR_SHARED_PTR<ServiceInfo2Subscription>, const ServiceSubscriptionClientID&, const ServiceInfo2&)> handler);

		virtual void Close();

	protected:

		bool active;

		std::map<ServiceSubscriptionClientID, RR_SHARED_PTR<detail::ServiceInfo2Subscription_client> > clients;

		RR_WEAK_PTR<RobotRaconteurNode> node;
		RR_WEAK_PTR<detail::Discovery> parent;

		std::vector<std::string> service_types;
		RR_SHARED_PTR<ServiceSubscriptionFilter> filter;

		uint32_t retry_delay;

		boost::signals2::signal<void(RR_SHARED_PTR<ServiceInfo2Subscription>, const ServiceSubscriptionClientID&, const ServiceInfo2&)> detected_listeners;
		boost::signals2::signal<void(RR_SHARED_PTR<ServiceInfo2Subscription>, const ServiceSubscriptionClientID&, const ServiceInfo2&)> lost_listeners;

	public:
		//Do not call, use RobotRaconteurNode()->SubscribeServiceInfo2()
		ServiceInfo2Subscription(RR_SHARED_PTR<detail::Discovery> parent);

	protected:
		virtual void Init(const std::vector<std::string>& service_types, RR_SHARED_PTR<ServiceSubscriptionFilter> filter);
		virtual void NodeUpdated(RR_SHARED_PTR<detail::Discovery_nodestorage> storage);
		virtual void NodeLost(RR_SHARED_PTR<detail::Discovery_nodestorage> storage);
		
		void fire_ServiceDetectedListener(const ServiceSubscriptionClientID& noden, const ServiceInfo2& info);
		void fire_ServiceLostListener(const ServiceSubscriptionClientID& noden, const ServiceInfo2& info);

	};

	class ROBOTRACONTEUR_CORE_API ServiceSubscription : public IServiceSubscription, public RR_ENABLE_SHARED_FROM_THIS<ServiceSubscription>, private boost::noncopyable
	{
	public:

		friend class detail::Discovery;
		friend class detail::ServiceSubscription_retrytimer;
		friend class WireSubscriptionBase;
		friend class PipeSubscriptionBase;
		friend class detail::ServiceSubscription_custom_member_subscribers;

		typedef boost::signals2::connection event_connection;
				
		std::map<ServiceSubscriptionClientID, RR_SHARED_PTR<RRObject> > GetConnectedClients();

		event_connection AddClientConnectListener(boost::function<void(RR_SHARED_PTR<ServiceSubscription>, const ServiceSubscriptionClientID&, RR_SHARED_PTR<RRObject>)> handler);
		event_connection AddClientDisconnectListener(boost::function<void(RR_SHARED_PTR<ServiceSubscription>, const ServiceSubscriptionClientID&, RR_SHARED_PTR<RRObject>)> handler);

		virtual void Close();

		virtual void ClaimClient(RR_SHARED_PTR<RRObject> client);
		virtual void ReleaseClient(RR_SHARED_PTR<RRObject> client);

		uint32_t GetConnectRetryDelay();
		void SetConnectRetryDelay(uint32_t delay_milliseconds);

		template <typename T>
		RR_SHARED_PTR<WireSubscription<T> > SubscribeWire(boost::string_ref membername, boost::string_ref servicepath = "")
		{
			RR_SHARED_PTR<WireSubscription<T> > o = RR_MAKE_SHARED<WireSubscription<T> >(shared_from_this(), membername, servicepath);
			SubscribeWire1(o);
			return o;
		}

		template <typename T>
		RR_SHARED_PTR<PipeSubscription<T> > SubscribePipe(boost::string_ref membername, boost::string_ref servicepath = "", uint32_t max_recv_packets=std::numeric_limits<uint32_t>::max())
		{
			RR_SHARED_PTR<PipeSubscription<T> > o = RR_MAKE_SHARED<PipeSubscription<T> >(shared_from_this(), membername, servicepath, max_recv_packets);
			SubscribePipe1(o);
			return o;
		}

		template <typename T>
		RR_SHARED_PTR<T> GetDefaultClient()
		{
			return rr_cast<T>(GetDefaultClientBase());
		}

		template <typename T>
		bool TryGetDefaultClient(RR_SHARED_PTR<T>& client_out)
		{
			RR_SHARED_PTR<RRObject> c;
			if (!TryGetDefaultClientBase(c))
			{
				return false;
			}
			RR_SHARED_PTR<T> c1 = RR_DYNAMIC_POINTER_CAST<T>(c);
			if (!c1)
				return false;

			client_out = c1;
			return true;
		}

	protected:

		bool active;

		std::map<ServiceSubscriptionClientID, RR_SHARED_PTR<detail::ServiceSubscription_client> > clients;
		
		RR_WEAK_PTR<RobotRaconteurNode> node;
		RR_WEAK_PTR<detail::Discovery> parent;

		std::vector<std::string> service_types;
		RR_SHARED_PTR<ServiceSubscriptionFilter> filter;
		
		uint32_t retry_delay;

		boost::signals2::signal<void(RR_SHARED_PTR<ServiceSubscription>, const ServiceSubscriptionClientID&, RR_SHARED_PTR<RRObject>)> connect_listeners;
		boost::signals2::signal<void(RR_SHARED_PTR<ServiceSubscription>, const ServiceSubscriptionClientID&, RR_SHARED_PTR<RRObject>)> disconnect_listeners;

		boost::unordered_set<RR_SHARED_PTR<WireSubscriptionBase> > wire_subscriptions;
		boost::unordered_set<RR_SHARED_PTR<PipeSubscriptionBase> > pipe_subscriptions;

		bool use_service_url;
		std::vector<std::string> service_url;
		std::string service_url_username;
		RR_INTRUSIVE_PTR<RRMap<std::string,RRValue> > service_url_credentials;

	public:
		//Do not call, use RobotRaconteurNode()->SubscribeService()
		ServiceSubscription(RR_SHARED_PTR<detail::Discovery> parent);
	protected:
		virtual void Init(const std::vector<std::string>& service_types, RR_SHARED_PTR<ServiceSubscriptionFilter> filter);
		virtual void InitServiceURL(const std::vector<std::string>& url, boost::string_ref username = "", RR_INTRUSIVE_PTR<RRMap<std::string,RRValue> > credentials=(RR_INTRUSIVE_PTR<RRMap<std::string,RRValue> >()),  boost::string_ref objecttype = "");
		virtual void NodeUpdated(RR_SHARED_PTR<detail::Discovery_nodestorage> storage);
		virtual void NodeLost(RR_SHARED_PTR<detail::Discovery_nodestorage> storage);

		void ClientConnected(RR_SHARED_PTR<RRObject> c, RR_SHARED_PTR<RobotRaconteurException> err, RR_SHARED_PTR<detail::ServiceSubscription_client> c2);
		void ConnectRetry(RR_SHARED_PTR<detail::ServiceSubscription_client> c2);
		void ConnectRetry2(RR_SHARED_PTR<detail::ServiceSubscription_client> c2);

		static void ClientEvent(RR_WEAK_PTR<ServiceSubscription> this_, RR_SHARED_PTR<ClientContext> ctx, ClientServiceListenerEventType evt, RR_SHARED_PTR<void> p, RR_WEAK_PTR<detail::ServiceSubscription_client> c2);

		void fire_ClientConnectListeners(const ServiceSubscriptionClientID& noden, RR_SHARED_PTR<RRObject> client);
		void fire_ClientDisconnectListeners(const ServiceSubscriptionClientID& noden, RR_SHARED_PTR<RRObject> client);

		void SubscribeWire1(RR_SHARED_PTR<WireSubscriptionBase> s);
		void SubscribePipe1(RR_SHARED_PTR<PipeSubscriptionBase> s);

		void WireSubscriptionClosed(RR_SHARED_PTR<WireSubscriptionBase> s);
		void PipeSubscriptionClosed(RR_SHARED_PTR<PipeSubscriptionBase> s);

		RR_SHARED_PTR<RRObject> GetDefaultClientBase();
		bool TryGetDefaultClientBase(RR_SHARED_PTR<RRObject>& client_out);
	};

	class ROBOTRACONTEUR_CORE_API WireSubscriptionBase : public RR_ENABLE_SHARED_FROM_THIS<WireSubscriptionBase>, private boost::noncopyable
	{
	public:
		friend class WireConnectionBase;
		friend class ServiceSubscription;
		friend class detail::WireSubscription_connection;
		friend class detail::WireSubscription_send_iterator;

		typedef boost::signals2::connection event_connection;

		RR_INTRUSIVE_PTR<RRValue> GetInValueBase(TimeSpec* time = NULL, RR_SHARED_PTR<WireConnectionBase>* connection = NULL);
		bool TryGetInValueBase(RR_INTRUSIVE_PTR<RRValue>& val, TimeSpec* time = NULL, RR_SHARED_PTR<WireConnectionBase>* connection = NULL);

		size_t GetActiveWireConnectionCount();

		bool GetIgnoreInValue();
		void SetIgnoreInValue(bool ignore);

		int32_t GetInValueLifespan();
		void SetInValueLifespan(int32_t millis);

		void SetOutValueAllBase(const RR_INTRUSIVE_PTR<RRValue>& val);

		size_t GetWireConnectionCount();

		void Close();

		WireSubscriptionBase(RR_SHARED_PTR<ServiceSubscription> parent, boost::string_ref membername, boost::string_ref servicepath);

	protected:

		void ClientConnected(RR_SHARED_PTR<RRObject> client);
		void ClientConnected1(RR_WEAK_PTR<RRObject> client, RR_SHARED_PTR<WireConnectionBase> connection, RR_SHARED_PTR<RobotRaconteurException> err);

		void WireConnectionClosed(RR_SHARED_PTR<detail::WireSubscription_connection> wire);
		void WireValueChanged(RR_SHARED_PTR<detail::WireSubscription_connection> wire, RR_INTRUSIVE_PTR<RRValue> value, const TimeSpec& time);
		boost::unordered_set<RR_SHARED_PTR<detail::WireSubscription_connection> > connections;
		boost::initialized<bool> closed;
		RR_WEAK_PTR<RobotRaconteurNode> node;
		RR_WEAK_PTR<ServiceSubscription> parent;

		RR_INTRUSIVE_PTR<RRValue> in_value;
		TimeSpec in_value_time;
		boost::initialized<bool> in_value_valid;
		RR_SHARED_PTR<WireConnectionBase> in_value_connection;
		int32_t in_value_lifespan;
		boost::posix_time::ptime in_value_time_local;

		
		boost::initialized<bool> ignore_in_value;

		std::string membername;
		std::string servicepath;

		virtual void fire_WireValueChanged(RR_INTRUSIVE_PTR<RRValue> value, const TimeSpec& time, RR_SHARED_PTR<WireConnectionBase> connection);
		virtual bool isempty_WireValueChanged();

	};

	template <typename T>
	class WireSubscription : public WireSubscriptionBase
	{
	public:

		WireSubscription(RR_SHARED_PTR<ServiceSubscription> parent, boost::string_ref membername, boost::string_ref servicepath)
			: WireSubscriptionBase(parent, membername, servicepath)
		{

		}

		T GetInValue(TimeSpec* time = NULL, typename RR_SHARED_PTR<WireConnection<T> >* connection = NULL)
		{
			RR_SHARED_PTR<WireConnectionBase> connection1;
			T o =  RRPrimUtil<T>::PreUnpack(GetInValueBase(time, &connection1));
			if (connection1)
			{
				*connection = RR_DYNAMIC_POINTER_CAST<WireConnection<T> >(connection1);
			}
			return o;
		}
		bool TryGetInValue(T& val, TimeSpec* time = NULL, typename RR_SHARED_PTR<WireConnection<T> >* connection = NULL)
		{
			RR_INTRUSIVE_PTR<RRValue> o;
			RR_SHARED_PTR<WireConnectionBase> connection1;
			if (!TryGetInValueBase(o, time, &connection1)) return false;
			val = RRPrimUtil<T>::PreUnpack(o);
			if (connection && connection1)
			{
				*connection = RR_DYNAMIC_POINTER_CAST<WireConnection<T> >(connection1);
			}
			return true;
		}

		void SetOutValueAll(const T& val)
		{
			SetOutValueAllBase(RRPrimUtil<T>::PrePack(val));
		}

		event_connection AddWireValueChangedListener(boost::function<void(RR_SHARED_PTR<WireSubscription<T> >, const T&, const TimeSpec&)> f)
		{
			return wire_value_changed.connect(f);
		}

	protected:

		boost::signals2::signal<void(RR_SHARED_PTR<WireSubscription<T> >, const T&, const TimeSpec&)> wire_value_changed;

		virtual void fire_WireValueChanged(RR_INTRUSIVE_PTR<RRValue> value, const TimeSpec& time, RR_SHARED_PTR<WireConnectionBase> connection)
		{
			wire_value_changed(RR_STATIC_POINTER_CAST<WireSubscription<T> >(shared_from_this()), RRPrimUtil<T>::PreUnpack(value), time);
		}

		virtual bool isempty_WireValueChanged()
		{
			return wire_value_changed.empty();
		}

	};

	class ROBOTRACONTEUR_CORE_API PipeSubscriptionBase : public RR_ENABLE_SHARED_FROM_THIS<PipeSubscriptionBase>, private boost::noncopyable
	{
	public:
		friend class PipeConnectionBase;
		friend class ServiceSubscription;
		friend class detail::PipeSubscription_connection;
		friend class detail::PipeSubscription_send_iterator;

		typedef boost::signals2::connection event_connection;

		RR_INTRUSIVE_PTR<RRValue> ReceivePacketBase();		
		bool TryReceivePacketBase(RR_INTRUSIVE_PTR<RRValue>& packet, bool peek = false, RR_SHARED_PTR<PipeEndpointBase>* ep = NULL );

		size_t Available();
		size_t GetActivePipeEndpointCount();

		bool GetIgnoreReceived();
		void SetIgnoreReceived(bool ignore);
				
		void AsyncSendPacketAllBase(const RR_INTRUSIVE_PTR<RRValue>& packet);

		size_t GetPipeEndpointCount();

		void Close();

		PipeSubscriptionBase(RR_SHARED_PTR<ServiceSubscription> parent, boost::string_ref membername, boost::string_ref servicepath = "", int32_t max_recv_packets = -1, int32_t max_send_backlog = 5);

	protected:

		void ClientConnected(RR_SHARED_PTR<RRObject> client);
		void ClientConnected1(RR_WEAK_PTR<RRObject> client, RR_SHARED_PTR<PipeEndpointBase> connection, RR_SHARED_PTR<RobotRaconteurException> err);

		void PipeEndpointClosed(RR_SHARED_PTR<detail::PipeSubscription_connection> pipe);
		void PipeEndpointPacketReceived(RR_SHARED_PTR<detail::PipeSubscription_connection> pipe, RR_INTRUSIVE_PTR<RRValue> packet);
		boost::unordered_set<RR_SHARED_PTR<detail::PipeSubscription_connection> > connections;
		boost::initialized<bool> closed;
		RR_WEAK_PTR<ServiceSubscription> parent;
		RR_WEAK_PTR<RobotRaconteurNode> node;

		std::deque<boost::tuple<RR_INTRUSIVE_PTR<RRValue>, RR_SHARED_PTR<PipeEndpointBase> > > recv_packets;
		
		std::string membername;
		std::string servicepath;

		boost::initialized<int32_t> max_recv_packets;
		boost::initialized<bool> ignore_incoming_packets;

		boost::initialized<int32_t> max_send_backlog;

		virtual void fire_PipePacketReceived();
		virtual bool isempty_PipePacketReceived();
	};

	template <typename T>
	class PipeSubscription : public PipeSubscriptionBase
	{
	public:

		PipeSubscription(RR_SHARED_PTR<ServiceSubscription> parent, boost::string_ref membername, boost::string_ref servicepath = "", int32_t max_recv_packets = -1, int32_t max_send_backlog = 5)
			: PipeSubscriptionBase(parent, membername, servicepath, max_recv_packets)
		{

		}

		T ReceivePacket()
		{
			return RRPrimUtil<T>::PreUnpack(ReceivePacketBase());
		}
		bool TryReceivePacket(T& packet)
		{
			RR_INTRUSIVE_PTR<RRValue> o;
			if (!TryReceivePacketBase(o)) return false;
			packet = RRPrimUtil<T>::PreUnpack(o);
			return true;
		}

		bool TryReceivePacket(T& packet, bool peek = false, RR_SHARED_PTR<PipeEndpoint<T> >* ep = NULL)
		{
			RR_INTRUSIVE_PTR<RRValue> o;
			RR_SHARED_PTR<PipeEndpointBase> ep1;
			if (!TryReceivePacketBase(o, peek, &ep1)) return false;
			packet = RRPrimUtil<T>::PreUnpack(o);
			if (ep1)
			{				
				*ep = RR_DYNAMIC_POINTER_CAST<PipeEndpoint<T> >(ep1);				
			}
			return true;
		}

		void AsyncSendPacketAll(const T& packet)
		{
			AsyncSendPacketAllBase(RRPrimUtil<T>::PrePack(packet));
		}

		event_connection AddPipePacketReceivedListener(boost::function<void(RR_SHARED_PTR<PipeSubscription<T> >)> f)
		{
			return pipe_packet_received.connect(f);
		}

	protected:

		boost::signals2::signal<void(RR_SHARED_PTR<PipeSubscription<T> >)> pipe_packet_received;

		virtual void fire_PipePacketReceived()
		{
			pipe_packet_received(RR_STATIC_POINTER_CAST<PipeSubscription<T> >(shared_from_this()));
		}

		virtual bool isempty_PipePacketReceived()
		{
			return pipe_packet_received.empty();
		}

	};

	namespace detail
	{
		class ROBOTRACONTEUR_CORE_API WireSubscription_send_iterator
		{
		protected:
			RR_SHARED_PTR<WireSubscriptionBase> subscription;
			boost::unordered_set<RR_SHARED_PTR<WireSubscription_connection> >::iterator connections_iterator;
			boost::unordered_set<RR_SHARED_PTR<WireSubscription_connection> >::iterator current_connection;

		public:
			WireSubscription_send_iterator(RR_SHARED_PTR<WireSubscriptionBase> subscription);
			RR_SHARED_PTR<WireConnectionBase> Next();
			void SetOutValue(const RR_INTRUSIVE_PTR<RRValue>& value);
			virtual ~WireSubscription_send_iterator();

		};
	
		class ROBOTRACONTEUR_CORE_API PipeSubscription_send_iterator
		{
		protected:
			RR_SHARED_PTR<PipeSubscriptionBase> subscription;
			boost::unordered_set<RR_SHARED_PTR<PipeSubscription_connection> >::iterator connections_iterator;
			boost::unordered_set<RR_SHARED_PTR<PipeSubscription_connection> >::iterator current_connection;

		public:
			PipeSubscription_send_iterator(RR_SHARED_PTR<PipeSubscriptionBase> subscription);
			RR_SHARED_PTR<PipeEndpointBase> Next();			
			void AsyncSendPacket(const RR_INTRUSIVE_PTR<RRValue>& packet);
			virtual ~PipeSubscription_send_iterator();

		};

		class ROBOTRACONTEUR_CORE_API ServiceSubscription_custom_member_subscribers
		{
		public:

			static void SubscribeWire(RR_SHARED_PTR<ServiceSubscription> s, RR_SHARED_PTR<WireSubscriptionBase> o);
			static void SubscribePipe(RR_SHARED_PTR<ServiceSubscription> s, RR_SHARED_PTR<PipeSubscriptionBase> o);

		};
	}

#ifndef BOOST_NO_CXX11_TEMPLATE_ALIASES
	using ServiceSubscriptionFilterNodePtr = RR_SHARED_PTR<ServiceSubscriptionFilterNode>;
	using ServiceSubscriptionFilterPtr = RR_SHARED_PTR<ServiceSubscriptionFilter>;
	using ServiceInfo2SubscriptionPtr = RR_SHARED_PTR<ServiceInfo2Subscription>;
	using ServiceSubscriptionPtr = RR_SHARED_PTR<ServiceSubscription>;
	template<typename T> using PipeSubscriptionPtr = RR_SHARED_PTR<PipeSubscription<T> >;
	template<typename T> using WireSubscriptionPtr = RR_SHARED_PTR<WireSubscription<T> >;

#endif

}
