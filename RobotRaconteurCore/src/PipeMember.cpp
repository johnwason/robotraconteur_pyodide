// Copyright 2011-2018 Wason Technology, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifdef ROBOTRACONTEUR_CORE_USE_STDAFX
#include "stdafx.h"
#endif

#include "PipeMember_private.h"
#include "RobotRaconteur/RobotRaconteurNode.h"
#include "RobotRaconteur/Client.h"
#include "RobotRaconteur/Service.h"
#include "RobotRaconteur/DataTypes.h"

#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/foreach.hpp>

#define RR_PIPE_ENDPOINT_LISTENER_ITER(command) \
		try \
		{ \
		boost::mutex::scoped_lock listen_lock(listeners_lock); \
		for (std::list<RR_WEAK_PTR<PipeEndpointBaseListener> >::iterator e = listeners.begin(); e != listeners.end();) \
		{ \
			RR_SHARED_PTR<PipeEndpointBaseListener> p1 = e->lock(); \
			if (!p1) \
			{ \
				e = listeners.erase(e); \
				continue; \
			} \
			command; \
			e++; \
		} \
		} \
		catch (std::exception& exp) \
		{ \
			RobotRaconteurNode::TryHandleException(node, &exp); \
		} \


namespace RobotRaconteur
{
	void PipeMember_empty_handler(RR_SHARED_PTR<RobotRaconteurException>) {}

PipeEndpointBase::PipeEndpointBase(RR_SHARED_PTR<PipeBase> parent, int32_t index, uint32_t endpoint, bool unreliable, MemberDefinition_Direction direction, bool message3)
{
	send_packet_number=0;
	recv_packet_number=0;

	this->index=index;
	this->parent=parent;
	this->endpoint=endpoint;
	this->unreliable=unreliable;
	this->direction = direction;

	RequestPacketAck=false;
	ignore_incoming_packets = false;
	this->message3 = message3;
	closed = false;
	this->node = parent->GetNode();

}

RR_SHARED_PTR<RobotRaconteurNode> PipeEndpointBase::GetNode()
{
	return GetParent()->GetNode();
}

int32_t PipeEndpointBase::GetIndex()
{
	return index;
}

uint32_t PipeEndpointBase::GetEndpoint()
{
	return endpoint;
}

size_t PipeEndpointBase::Available()
{
	boost::mutex::scoped_lock lock(recvlock);
	return recv_packets.size();
}

void PipeEndpointBase::Close()
{
	RR_SHARED_PTR<detail::sync_async_handler<void> > t=RR_MAKE_SHARED<detail::sync_async_handler<void > >();
	AsyncClose(boost::bind(&detail::sync_async_handler<void>::operator(),t,_1),GetNode()->GetRequestTimeout());
	t->end_void();
}

void PipeEndpointBase::AsyncClose(RR_MOVE_ARG(boost::function<void(RR_SHARED_PTR<RobotRaconteurException>)>) handler, int32_t timeout)
{
	{
		boost::mutex::scoped_lock lock(recvlock);
		//recv_packets.clear();
		closed = true;
		recv_packets_wait.notify_all();
	}

	{
	boost::mutex::scoped_lock lock (sendlock);
	GetParent()->AsyncClose(shared_from_this(),false,endpoint,RR_MOVE(handler),timeout);
	}
	
}

bool PipeEndpointBase::IsUnreliable()
{
	return unreliable;
}

MemberDefinition_Direction PipeEndpointBase::Direction()
{
	return direction;
}

void PipeEndpointBase_RemoteClose_emptyhandler(RR_SHARED_PTR<RobotRaconteurException> err) {}

void PipeEndpointBase::RemoteClose()
{
	{
		boost::mutex::scoped_lock lock(recvlock);
		closed = true;
		recv_packets_wait.notify_all();
	}

	RR_PIPE_ENDPOINT_LISTENER_ITER(p1->PipeEndpointClosed(shared_from_this()));

	try
	{
	fire_PipeEndpointClosedCallback();
	}
	catch (std::exception&) {};
		try
		{
			boost::mutex::scoped_lock lock (sendlock);
			//if (parent.expired()) return;
			//boost::mutex::scoped_lock lock2 (recvlock);
			GetParent()->AsyncClose(shared_from_this(),true,endpoint,&PipeEndpointBase_RemoteClose_emptyhandler,1000);
		}
		catch (std::exception&)
		{
		}
}

void PipeEndpointBase::AsyncSendPacketBase(RR_SHARED_PTR<RRValue> packet, RR_MOVE_ARG(boost::function<void(uint32_t,RR_SHARED_PTR<RobotRaconteurException>)>) handler)
{ 
	if (direction == MemberDefinition_Direction_readonly)
	{
		throw ReadOnlyMemberException("Read only pipe");
	}
	
	{
		boost::mutex::scoped_lock lock(sendlock);
		send_packet_number = (send_packet_number < UINT_MAX) ? send_packet_number + 1 : 0;

		GetParent()->AsyncSendPipePacket(packet, index, send_packet_number, RequestPacketAck, endpoint,unreliable,message3,RR_MOVE(handler));
		
	}


}

RR_SHARED_PTR<RRValue> PipeEndpointBase::ReceivePacketBase()
{	
	RR_SHARED_PTR<RRValue> o;
	if (!TryReceivePacketBaseWait(o, 0))
	{
		throw InvalidOperationException("Pipe endpoint receive queue is empty");
	}
	return o;	
}

RR_SHARED_PTR<RRValue> PipeEndpointBase::PeekPacketBase()
{
	RR_SHARED_PTR<RRValue> o;
	if (!TryReceivePacketBaseWait(o, 0, true))
	{
		throw InvalidOperationException("Pipe endpoint receive queue is empty");
	}
	return o;
}

RR_SHARED_PTR<RRValue> PipeEndpointBase::ReceivePacketBaseWait(int32_t timeout)
{
	RR_SHARED_PTR<RRValue> o;
	if (!TryReceivePacketBaseWait(o, timeout))
	{
		throw InvalidOperationException("Pipe endpoint receive queue is empty");
	}
	return o;
}
RR_SHARED_PTR<RRValue> PipeEndpointBase::PeekPacketBaseWait(int32_t timeout)
{
	RR_SHARED_PTR<RRValue> o;
	if (!TryReceivePacketBaseWait(o, timeout, true))
	{
		throw InvalidOperationException("Pipe endpoint receive queue is empty");
	}
	return o;
}

bool PipeEndpointBase::TryReceivePacketBaseWait(RR_SHARED_PTR<RRValue>& packet, int32_t timeout, bool peek)
{
	if (direction == MemberDefinition_Direction_writeonly)
	{
		throw WriteOnlyMemberException("Write only pipe");
	}

	boost::mutex::scoped_lock lock(recvlock);	
	if (recv_packets.empty())
	{
		if (timeout == 0)
		{
			return false;
		}

		if (closed) return false;

		if (timeout < 0)
		{			
			recv_packets_wait.wait(lock);
		}
		else
		{			
			recv_packets_wait.wait_for(lock, boost::chrono::milliseconds(timeout));
		}

		if (recv_packets.empty()) return false;
	}

	packet = recv_packets.front();
	if (!peek)
	{
		recv_packets.pop_front();
	}
	return true;

}

static bool PipeEndpointBase_PipePacketReceived_recvpacket(std::deque<RR_SHARED_PTR<RRValue> >& q, RR_SHARED_PTR<RRValue>& packet)
{
	std::deque<RR_SHARED_PTR<RRValue> >::iterator e = q.begin();
	if (e == q.end()) return false;
	packet = *e;
	q.pop_front();
	return true;
}

void PipeEndpointBase::PipePacketReceived(RR_SHARED_PTR<RRValue> packet, uint32_t packetnum)
{	
	if (direction == MemberDefinition_Direction_writeonly)
	{
		return;
	}

	if (unreliable)
	{
		boost::mutex::scoped_lock lock (recvlock);
		if (ignore_incoming_packets) return;
		recv_packets.push_back(packet);
						
		RR_PIPE_ENDPOINT_LISTENER_ITER(p1->PipePacketReceived(shared_from_this(), boost::bind(&PipeEndpointBase_PipePacketReceived_recvpacket, boost::ref(recv_packets), _1)));
		
		if (!recv_packets.empty())
		{
			recv_packets_wait.notify_all();

			{
				try
				{
					lock.unlock();
					pipe_packet_received_semaphore.try_fire_next(
					boost::bind(&PipeEndpointBase::fire_PacketReceivedEvent,this));

				}
				catch (std::exception&)
				{
				}

			}
		}
		return;
	}
	else
	{
		{
			boost::mutex::scoped_lock lock (recvlock);
			if (ignore_incoming_packets) return;
			if (packetnum == increment_packet_number(recv_packet_number))
			{
				recv_packets.push_back(packet);
				recv_packet_number = increment_packet_number(recv_packet_number);
				if (out_of_order_packets.size() > 0)
				{
					while (out_of_order_packets.find(increment_packet_number(recv_packet_number))!= out_of_order_packets.end())
					{
						recv_packet_number = increment_packet_number(recv_packet_number);
						RR_SHARED_PTR<RRValue> opacket = out_of_order_packets[recv_packet_number];
						recv_packets.push_back(opacket);
						out_of_order_packets.erase(recv_packet_number);

					}

				}

				RR_PIPE_ENDPOINT_LISTENER_ITER(p1->PipePacketReceived(shared_from_this(), boost::bind(&PipeEndpointBase_PipePacketReceived_recvpacket, boost::ref(recv_packets), _1)));

				if (!recv_packets.empty())
				{
					recv_packets_wait.notify_all();

					//if (PacketReceivedEvent != 0)
					{
						try
						{
							lock.unlock();
							pipe_packet_received_semaphore.try_fire_next(
							boost::bind(&PipeEndpointBase::fire_PacketReceivedEvent,this));
						}
						catch (std::exception&)
						{
						}

					}
				}
			}
			else
			{
				out_of_order_packets.insert(std::make_pair(packetnum, packet));
			}
		}
	}
}

void PipeEndpointBase::PipePacketAckReceived(uint32_t packetnum)
{
	RR_PIPE_ENDPOINT_LISTENER_ITER(p1->PipePacketAckReceived(shared_from_this(), packetnum));

	fire_PacketAckReceivedEvent(packetnum);
}

uint32_t PipeEndpointBase::increment_packet_number(uint32_t packetnum)
{
	return (packetnum < std::numeric_limits<uint32_t>::max()) ? packetnum + 1 : 0;

}

RR_SHARED_PTR<PipeBase> PipeEndpointBase::GetParent()
{
	RR_SHARED_PTR<PipeBase> out=parent.lock();
	if (!out) throw InvalidOperationException("Pipe endpoint has been closed");
	return out;
}

bool PipeEndpointBase::GetRequestPacketAck()
{
	boost::mutex::scoped_lock lock (sendlock);
	return RequestPacketAck;
}

void PipeEndpointBase::SetRequestPacketAck(bool ack)
{
	boost::mutex::scoped_lock lock(sendlock);
	RequestPacketAck=ack;
}

bool PipeEndpointBase::GetIgnoreReceived()
{
	boost::mutex::scoped_lock lock(recvlock);
	return ignore_incoming_packets;
}
void PipeEndpointBase::SetIgnoreReceived(bool ignore)
{
	boost::mutex::scoped_lock lock(recvlock);
	if (!ignore && ignore_incoming_packets) throw InvalidOperationException("Cannot stop ignoring packets");
	ignore_incoming_packets = ignore;
}

void PipeEndpointBase::AddListener(RR_SHARED_PTR<PipeEndpointBaseListener> listener)
{
	boost::mutex::scoped_lock lock(listeners_lock);
	listeners.push_back(listener);
}

void PipeEndpointBase::Shutdown()
{
	boost::mutex::scoped_lock lock(recvlock);
	closed = true;
	recv_packets_wait.notify_all();

	RobotRaconteurNode::TryPostToThreadPool(node, boost::bind(&PipeEndpointBase::fire_PipeEndpointClosedCallback, shared_from_this()));

	std::list<RR_WEAK_PTR<PipeEndpointBaseListener> > listeners1;
	{
		boost::mutex::scoped_lock lock(listeners_lock);
		listeners.swap(listeners1);
	}

	BOOST_FOREACH(RR_WEAK_PTR<PipeEndpointBaseListener> l, listeners1)
	{
		RR_SHARED_PTR<PipeEndpointBaseListener> l1 = l.lock();
		if (l1)
		{
			RobotRaconteurNode::TryPostToThreadPool(node, boost::bind(&PipeEndpointBaseListener::PipeEndpointClosed, l1, shared_from_this()));
		}
	}
}

void PipeBase::DispatchPacketAck (RR_SHARED_PTR<MessageElement> me, RR_SHARED_PTR<PipeEndpointBase> e)
{
	uint32_t pnum;
	if (me->ElementFlags & MessageElementFlags_SEQUENCE_NUMBER)
	{
		pnum = me->SequenceNumber;
	}
	else
	{
		pnum = RRArrayToScalar(me->CastData<RRArray<uint32_t> >());
	}
	e->PipePacketAckReceived(pnum);
}

bool PipeBase::DispatchPacket(RR_SHARED_PTR<MessageElement> me, RR_SHARED_PTR<PipeEndpointBase> e, uint32_t& packetnumber)
{
	//int32_t index=boost::lexical_cast<int32_t>(me->ElementName);
	if ((me->ElementFlags & MessageElementFlags_ELEMENT_NUMBER) && (me->ElementFlags & MessageElementFlags_SEQUENCE_NUMBER))
	{
		//Use message 3
		packetnumber = me->SequenceNumber;
		RR_SHARED_PTR<RRValue> data;
		if (!rawelements)
		{
			data = UnpackData(me);
		}
		else
		{
			data = me;
		}

		e->PipePacketReceived(data, packetnumber);

		return (me->ElementFlags & MessageElementFlags_REQUEST_ACK)!=0;
	}
	else
	{
		//Use message 2
		RR_SHARED_PTR<MessageElementMap<std::string> > elems1 = me->CastData<MessageElementMap<std::string> >();
		packetnumber = RRArrayToScalar(MessageElement::FindElement(elems1->Elements, "packetnumber")->CastData<RRArray<uint32_t> >());

		RR_SHARED_PTR<RRValue> data;
		if (!rawelements)
		{
			data = UnpackData(MessageElement::FindElement(elems1->Elements, "packet"));
		}
		else
		{
			data = MessageElement::FindElement(elems1->Elements, "packet");
		}

		e->PipePacketReceived(data, packetnumber);

		RR_SHARED_PTR<MessageElement> e;
		if (MessageElement::TryFindElement(elems1->Elements, "requestack", e))
		{
			return true;
		}		
	}

	return false;

}

RR_SHARED_PTR<MessageElement> PipeBase::PackPacket(RR_SHARED_PTR<RRValue> data, int32_t index, uint32_t packetnumber, bool requestack, bool message3)
{	
	if (message3)
	{
		//Use message 3		
		RR_SHARED_PTR<MessageElement> me;
		if (!rawelements)
		{
			me = RR_MAKE_SHARED<MessageElement>();
			me->SetData(PackData(data));
		}
		else
		{
			me = RR_MAKE_SHARED<MessageElement>();
			RR_SHARED_PTR<MessageElement> me2 = rr_cast<MessageElement>(data);
			me->SetData(me2->GetData());
		}

		me->ElementFlags = MessageElementFlags_ELEMENT_NUMBER | MessageElementFlags_SEQUENCE_NUMBER;
		me->ElementNumber = index;
		me->SequenceNumber = packetnumber;

		if (requestack)
		{
			me->ElementFlags |= MessageElementFlags_REQUEST_ACK;
		}

		return me;
	}
	else
	{
		//Use message 2
		std::vector<RR_SHARED_PTR<MessageElement> > elems;
		elems.push_back(RR_MAKE_SHARED<MessageElement>("packetnumber", ScalarToRRArray(packetnumber)));

		if (!rawelements)
		{
			RR_SHARED_PTR<MessageElementData> pdata = PackData(data);
			elems.push_back(RR_MAKE_SHARED<MessageElement>("packet", pdata));
		}
		else
		{
			RR_SHARED_PTR<MessageElement> pme = rr_cast<MessageElement>(data);
			pme->ElementName = "packet";
			elems.push_back(pme);
		}

		if (requestack)
		{
			elems.push_back(RR_MAKE_SHARED<MessageElement>("requestack", ScalarToRRArray((uint32_t)1)));
		}

		RR_SHARED_PTR<MessageElementMap<std::string> > delems = RR_MAKE_SHARED<MessageElementMap<std::string> >(elems);
		RR_SHARED_PTR<MessageElement> me = RR_MAKE_SHARED<MessageElement>(boost::lexical_cast<std::string>(index), delems);

		return me;
	}

}

RR_SHARED_PTR<RobotRaconteurNode> PipeBase::GetNode()
{
	RR_SHARED_PTR<RobotRaconteurNode> n=node.lock();
	if (!n) throw InvalidOperationException("Node has been released");
	return n;
}

MemberDefinition_Direction PipeBase::Direction()
{
	return direction;
}

bool PipeBase::IsUnreliable()
{
	return unreliable;
}

std::string PipeClientBase::GetMemberName()
{
	return m_MemberName;
}

void PipeClientBase::PipePacketReceived(RR_SHARED_PTR<MessageEntry> m, uint32_t e)
{
	//boost::shared_lock<boost::shared_mutex> lock(stub_lock);
	
	if (m->EntryType == MessageEntryType_PipeClosed)
	{
		try
		{
			

			int32_t index = RRArrayToScalar(m->FindElement("index")->CastData<RRArray<int32_t> >());
			RR_SHARED_PTR<PipeEndpointBase> p;
			{
				boost::mutex::scoped_lock lock(pipeendpoints_lock);
				RR_UNORDERED_MAP<int32_t, RR_SHARED_PTR<PipeEndpointBase> >::iterator e1 = pipeendpoints.find(index);
				if (e1 == pipeendpoints.end()) return;
				p = e1->second;
				pipeendpoints.erase(e1);				
			}
			p->RemoteClose();
			
		}
		catch (std::exception&)
		{
		};
	}

	else if (m->EntryType==MessageEntryType_PipeClosedRet)
	{
		try
		{
			boost::mutex::scoped_lock lock(pipeendpoints_lock);
			int32_t index = RRArrayToScalar(m->FindElement("index")->CastData<RRArray<int32_t> >());
			
			pipeendpoints.erase(index);
		}
		catch (std::exception&)
		{
		};	
	}

	else if (m->EntryType == MessageEntryType_PipePacket)
	{
		std::vector<RR_SHARED_PTR<MessageElement> > ack;;
		BOOST_FOREACH (RR_SHARED_PTR<MessageElement>& me, m->elements)
		{
			try
			{				
				int32_t index;
				if (me->ElementFlags & MessageElementFlags_ELEMENT_NUMBER)
				{
					index = me->ElementNumber;
				}
				else
				{
					index = boost::lexical_cast<int32_t>(me->ElementName);
				}
				uint32_t pnum;
				RR_SHARED_PTR<PipeEndpointBase> e;
				{
					boost::mutex::scoped_lock lock(pipeendpoints_lock);
					RR_UNORDERED_MAP<int32_t, RR_SHARED_PTR<PipeEndpointBase> >::iterator e1;
					if ((e1=pipeendpoints.find(index)) != pipeendpoints.end())
					{
						e = e1->second;
					}
					else if ((e1=early_endpoints.find(index)) != early_endpoints.end())
					{
						e = e1->second;
					}
					else
					{
						if (connecting_endpoints.size() == 0)
						{
							continue;
						}

						bool found = false;

						typedef boost::tuple<int32_t, int32_t> e2_type;
						BOOST_FOREACH (e2_type& e2, connecting_endpoints)	
						{
							if (e2.get<1>() == index || e2.get<1>() == -1)
								found = true;
						}

						if (!found)
						{
							continue;
						}

						RR_SHARED_PTR<PipeEndpointBase> new_ep = CreateNewPipeEndpoint(index, false, direction, GetStub()->GetContext()->UseMessage3());
						early_endpoints.insert(std::make_pair(index, new_ep));
						e = new_ep;
					}					
				}

				if (DispatchPacket(me, e, pnum))
				{		
					if (me->ElementFlags & MessageElementFlags_ELEMENT_NUMBER)
					{
						RR_SHARED_PTR<MessageElement> me1 = RR_MAKE_SHARED<MessageElement>();
						me1->SequenceNumber=((uint32_t)pnum);
						me1->ElementNumber = me->ElementNumber;
						me1->ElementFlags = MessageElementFlags_ELEMENT_NUMBER | MessageElementFlags_SEQUENCE_NUMBER;
						ack.push_back(me1);
					}
					else
					{
						ack.push_back(RR_MAKE_SHARED<MessageElement>(me->ElementName, ScalarToRRArray((uint32_t)pnum)));
					}
				}
			}

			catch (std::exception& e)
			{
				RobotRaconteurNode::TryHandleException(node, &e);
			}

		}
		try
		{
			if (ack.size() > 0)
			{				
				RR_SHARED_PTR<MessageEntry> mack = RR_MAKE_SHARED<MessageEntry>(MessageEntryType_PipePacketRet, m->MemberName);
				mack->elements = ack;
				if (!(ack.at(0)->ElementFlags & MessageElementFlags_ELEMENT_NUMBER))
				{
					if (unreliable)
					{
						mack->MetaData = "unreliable\n";
					}
				}
				boost::function<void(RR_SHARED_PTR<RobotRaconteurException>)> h = boost::bind(&PipeMember_empty_handler, _1);
				GetStub()->AsyncSendPipeMessage(mack,unreliable,h);

			}
		}
		catch (std::exception& e)
		{
			RobotRaconteurNode::TryHandleException(node, &e);
		}
		}
		else if (m->EntryType == MessageEntryType_PipePacketRet)
		{

		try
		{
			

			BOOST_FOREACH (RR_SHARED_PTR<MessageElement>& me, m->elements)
			{
				int32_t index;
				if (me->ElementFlags & MessageElementFlags_ELEMENT_NUMBER)
				{
					index = me->ElementNumber;
				}
				else
				{
					index = boost::lexical_cast<int32_t>(me->ElementName);
				}

				RR_SHARED_PTR<PipeEndpointBase> e;
				{
					boost::mutex::scoped_lock lock(pipeendpoints_lock);
					RR_UNORDERED_MAP<int32_t, RR_SHARED_PTR<PipeEndpointBase> >::iterator e1 = pipeendpoints.find(index);
					if (e1 == pipeendpoints.end()) return;
					e = e1->second;
				}
				DispatchPacketAck(me, e);
			}
		}
		catch (std::exception&)
		{
			
		}

	}
}

void PipeClientBase::Shutdown()
{
	std::vector<RR_SHARED_PTR<PipeEndpointBase> > p;
	{
		boost::mutex::scoped_lock lock2(pipeendpoints_lock);
		boost::copy(pipeendpoints | boost::adaptors::map_values, std::back_inserter(p));
		pipeendpoints.clear();
	}
	
	BOOST_FOREACH (RR_SHARED_PTR<PipeEndpointBase>& e, p)
	{
		try
		{		
		e->Shutdown();
		}
		catch (std::exception& e) 
		{
			RobotRaconteurNode::TryHandleException(node, &e);
		}		
	}

	//stub.reset();
}
		
void PipeClientBase::AsyncSendPipePacket(RR_SHARED_PTR<RRValue> data, int32_t index, uint32_t packetnumber, bool requestack, uint32_t endpoint, bool unreliable, bool message3, RR_MOVE_ARG(boost::function<void(uint32_t,RR_SHARED_PTR<RobotRaconteurException>)>) handler)
{	
	RR_SHARED_PTR<MessageElement> me = PackPacket(data, index, packetnumber, requestack, message3);
	RR_SHARED_PTR<MessageEntry> m = RR_MAKE_SHARED<MessageEntry>(MessageEntryType_PipePacket, GetMemberName());
	m->AddElement(me);
	if (!message3)
	{
		if (unreliable)
			m->MetaData = "unreliable\n";
	}
	boost::function<void(RR_SHARED_PTR<RobotRaconteurException>)> h = boost::bind(handler, packetnumber, _1);
	GetStub()->AsyncSendPipeMessage(m,unreliable, h);
}

void PipeClientBase::AsyncClose(RR_SHARED_PTR<PipeEndpointBase> endpoint, bool remote, uint32_t ee, RR_MOVE_ARG(boost::function<void (RR_SHARED_PTR<RobotRaconteurException>)>) handler, int32_t timeout)
{
	
	if (!remote)
	{
	RR_SHARED_PTR<MessageEntry> m = RR_MAKE_SHARED<MessageEntry>(MessageEntryType_PipeDisconnectReq, GetMemberName());
	m->AddElement("index", ScalarToRRArray(endpoint->GetIndex()));
	GetStub()->AsyncProcessRequest(m,boost::bind(handler,_2),timeout);
	}
}


void PipeClientBase::AsyncConnect_internal(int32_t index, RR_MOVE_ARG(boost::function<void (RR_SHARED_PTR<PipeEndpointBase>,RR_SHARED_PTR<RobotRaconteurException>)>) handler, int32_t timeout)
{
	
	boost::mutex::scoped_lock lock2(pipeendpoints_lock);

	connecting_key_count = connecting_key_count < std::numeric_limits<int32_t>::max() ? connecting_key_count + 1 : 0;
	int32_t key = connecting_key_count;
	boost::tuple<int, int> entry = boost::make_tuple(key, index);
	connecting_endpoints.push_back(entry);

	RR_SHARED_PTR<MessageEntry> m = RR_MAKE_SHARED<MessageEntry>(MessageEntryType_PipeConnectReq, GetMemberName());
	m->AddElement("index", ScalarToRRArray(index));

	if (unreliable)
		m->AddElement("unreliable",ScalarToRRArray((int32_t)1));

	lock2.unlock();
	GetStub()->AsyncProcessRequest(m,boost::bind(&PipeClientBase::AsyncConnect_internal1, RR_DYNAMIC_POINTER_CAST<PipeClientBase>(shared_from_this()),_1,_2,index,key,handler),timeout);
}

void PipeClientBase::AsyncConnect_internal1(RR_SHARED_PTR<MessageEntry> ret, RR_SHARED_PTR<RobotRaconteurException> err, int32_t index, int32_t key, boost::function<void (RR_SHARED_PTR<PipeEndpointBase>,RR_SHARED_PTR<RobotRaconteurException>)>& handler)
{
	boost::mutex::scoped_lock lock2(pipeendpoints_lock);

	boost::tuple<int, int> k = boost::make_tuple(key, index);

	for (std::list<boost::tuple<int32_t, int32_t> >::iterator e2 = connecting_endpoints.begin(); e2 != connecting_endpoints.end();)
	{
		if (e2->get<0>() == k.get<0>() && e2->get<1>() == k.get<1>())
		{
			e2=connecting_endpoints.erase(e2);
		}
		else
		{
			e2++;
		}
	}

	if (err)
	{
		try
		{
			if (connecting_endpoints.size() == 0)
			{
				early_endpoints.clear();
			}

			lock2.unlock();
			detail::InvokeHandlerWithException(node, handler, err);
			return;
		}
		catch (std::exception& e) 
		{
			RobotRaconteurNode::TryHandleException(node, &e);
			return;
		}
	}

	try
	{
		bool runreliable=false;

		try
		{
			if (RRArrayToScalar((ret->FindElement("unreliable")->CastData<RRArray<int32_t> >()))==1)
				runreliable=true;
		}
		catch (std::exception&) 
		{
			
		}

		int32_t rindex = RRArrayToScalar((ret->FindElement("index")->CastData<RRArray<int32_t> >()));

		RR_SHARED_PTR<PipeEndpointBase> e;
		
		RR_UNORDERED_MAP<int32_t, RR_SHARED_PTR<PipeEndpointBase> >::iterator e1 = early_endpoints.find(rindex);
		if (!runreliable)
		{
			if (e1!=early_endpoints.end())
			{
				e = e1->second;
				early_endpoints.erase(e1);
			}
		}
		else
		{
			if (e1 != early_endpoints.end())
			{
				early_endpoints.erase(e1);
			}
		}

		if (!e)
		{
			e = CreateNewPipeEndpoint(rindex, runreliable, direction, GetStub()->GetContext()->UseMessage3());
		}
						
		pipeendpoints.insert(std::make_pair(rindex, e));
		
		try
		{
			if (connecting_endpoints.size() == 0)
			{
				early_endpoints.clear();
			}

			lock2.unlock();
			detail::InvokeHandler(node, handler, e);
		}
		catch (std::exception& exp) {
			RobotRaconteurNode::TryHandleException(node, &exp);
		}
	}
	catch (std::exception& err2)
	{
		if (connecting_endpoints.size() == 0)
		{
			early_endpoints.clear();
		}

		lock2.unlock();
		detail::InvokeHandlerWithException(node, handler, err2);
		
	}

	
}

PipeClientBase::PipeClientBase(const std::string& name, RR_SHARED_PTR<ServiceStub> stub, bool unreliable, MemberDefinition_Direction direction)
{
	m_MemberName=name;
	this->stub=stub;
	this->unreliable=unreliable;
	this->direction = direction;
	this->node=stub->RRGetNode();	
	connecting_key_count = 0;
}

RR_SHARED_PTR<ServiceStub> PipeClientBase::GetStub()
{
	RR_SHARED_PTR<ServiceStub> out=stub.lock();
	if (!out) throw InvalidOperationException("Pipe has been closed");
	return out;
}

void PipeClientBase::DeleteEndpoint(RR_SHARED_PTR<PipeEndpointBase> e)
{
	
	boost::mutex::scoped_lock lock2(pipeendpoints_lock);
	pipeendpoints.erase(e->GetIndex());
}


std::string PipeServerBase::GetMemberName()
{
	return m_MemberName;
}

void PipeServerBase::PipePacketReceived(RR_SHARED_PTR<MessageEntry> m, uint32_t e)
{
	//boost::shared_lock<boost::shared_mutex> lock2(skel_lock);
	
	if (m->EntryType == MessageEntryType_PipePacket)
	{
		
		std::vector<RR_SHARED_PTR<MessageElement> > ack;
		BOOST_FOREACH (RR_SHARED_PTR<MessageElement>& me, m->elements)
		{
			try
			{
				int32_t index;
				if (me->ElementFlags & MessageElementFlags_ELEMENT_NUMBER)
				{
					index = me->ElementNumber;
				}
				else
				{
					index = boost::lexical_cast<int32_t>(me->ElementName);
				}
				uint32_t pnum;

				RR_SHARED_PTR<PipeEndpointBase> p;
				{
					boost::mutex::scoped_lock lock(pipeendpoints_lock);
					RR_UNORDERED_MAP<pipe_endpoint_server_id, RR_SHARED_PTR<PipeEndpointBase> > ::iterator e1 = pipeendpoints.find(pipe_endpoint_server_id(e, index));
					if (e1 == pipeendpoints.end()) return;
					p = e1->second;
				}
				if (DispatchPacket(me, p, pnum))
				{					
					if (me->ElementFlags & MessageElementFlags_ELEMENT_NUMBER)
					{
						RR_SHARED_PTR<MessageElement> me1 = RR_MAKE_SHARED<MessageElement>();
						me1->SequenceNumber = ((uint32_t)pnum);
						me1->ElementNumber = me->ElementNumber;
						me1->ElementFlags = MessageElementFlags_ELEMENT_NUMBER | MessageElementFlags_SEQUENCE_NUMBER;
						ack.push_back(me1);
					}
					else
					{
						ack.push_back(RR_MAKE_SHARED<MessageElement>(me->ElementName, ScalarToRRArray((uint32_t)pnum)));
					}
				}

			}
			catch (std::exception& exp)
			{
				RobotRaconteurNode::TryHandleException(node, &exp);
			}

		}

		try
		{
			if (ack.size() > 0)
			{				
				RR_SHARED_PTR<MessageEntry> mack = RR_MAKE_SHARED<MessageEntry>(MessageEntryType_PipePacketRet, m->MemberName);
				mack->elements = ack;
				if (!(ack.at(0)->ElementFlags & MessageElementFlags_ELEMENT_NUMBER))
				{
					if (unreliable)
					{
						mack->MetaData = "unreliable\n";
					}
				}
				GetSkel()->AsyncSendPipeMessage(mack, e, unreliable, boost::bind(&PipeMember_empty_handler,_1));

			}
		}
		catch (std::exception& exp)
		{
			RobotRaconteurNode::TryHandleException(node, &exp);
		}
	}
	else if (m->EntryType == MessageEntryType_PipePacketRet)
	{
		
		try
		{
			BOOST_FOREACH (RR_SHARED_PTR<MessageElement>& me, m->elements)
			{
				int32_t index;
				if (me->ElementFlags & MessageElementFlags_ELEMENT_NUMBER)
				{
					index = me->ElementNumber;
				}
				else
				{
					index = boost::lexical_cast<int32_t>(me->ElementName);
				}
				
				RR_SHARED_PTR<PipeEndpointBase> p;
				{
					boost::mutex::scoped_lock lock(pipeendpoints_lock);
					RR_UNORDERED_MAP<pipe_endpoint_server_id, RR_SHARED_PTR<PipeEndpointBase> > ::iterator e1 = pipeendpoints.find(pipe_endpoint_server_id(e, index));
					if (e1 == pipeendpoints.end()) return;
					p = e1->second;					
				}
				DispatchPacketAck(me, p);
			}
		}
		catch (std::exception&)
		{
			
		}

	}
}

void PipeServerBase::Shutdown()
{
	
	{
		std::vector<RR_SHARED_PTR<PipeEndpointBase> > p;
		{
			boost::mutex::scoped_lock lock(pipeendpoints_lock);
			
			boost::copy(pipeendpoints | boost::adaptors::map_values, std::back_inserter(p));			
			pipeendpoints.clear();
		}


		BOOST_FOREACH (RR_SHARED_PTR<PipeEndpointBase>& e, p)
		{	
				//Cycle through and close all endpoints
				try
				{

					RR_SHARED_PTR<MessageEntry> m = RR_MAKE_SHARED<MessageEntry>(MessageEntryType_PipeClosed, GetMemberName());
					m->AddElement("index", ScalarToRRArray(e->GetIndex()));
					GetSkel()->AsyncSendPipeMessage(m, e->GetEndpoint(), false, boost::bind(&PipeMember_empty_handler,_1));
				}
				catch (std::exception& exp)
				{
					RobotRaconteurNode::TryHandleException(node, &exp);
				}


				try
				{
					e->Shutdown();
				}
				catch (std::exception& exp) 
				{
					RobotRaconteurNode::TryHandleException(node, &exp);
				}

					//(endpoint)->second->RemoteClose();
		}

		

		//skel.reset();
		
		listener_connection.disconnect();
	}

}

void PipeServerBase::AsyncSendPipePacket(RR_SHARED_PTR<RRValue> data, int32_t index, uint32_t packetnumber, bool requestack, uint32_t e, bool unreliable, bool message3, RR_MOVE_ARG(boost::function<void(uint32_t,RR_SHARED_PTR<RobotRaconteurException>)>) handler)
{
	
	
	{
	boost::mutex::scoped_lock lock(pipeendpoints_lock);

	if (pipeendpoints.find(pipe_endpoint_server_id(e,index)) == pipeendpoints.end())
		throw InvalidOperationException("Pipe has been disconnect");
	}

	RR_SHARED_PTR<MessageElement> me = PackPacket(data, index, packetnumber, requestack, message3);
	RR_SHARED_PTR<MessageEntry> m = RR_MAKE_SHARED<MessageEntry>(MessageEntryType_PipePacket, GetMemberName());
	m->AddElement(me);

	if (!message3)
	{
		if (unreliable)
			m->MetaData = "unreliable\n";
	}

	GetSkel()->AsyncSendPipeMessage(m, e, unreliable, boost::bind(handler,packetnumber,_1));
}

void PipeServerBase::AsyncClose(RR_SHARED_PTR<PipeEndpointBase> e, bool remote, uint32_t ee, RR_MOVE_ARG(boost::function<void (RR_SHARED_PTR<RobotRaconteurException>)>) handler, int32_t timeout)
{
	
	if (!remote)
	{
	RR_SHARED_PTR<MessageEntry> m = RR_MAKE_SHARED<MessageEntry>(MessageEntryType_PipeClosed, GetMemberName());
	m->AddElement("index", ScalarToRRArray(e->GetIndex()));
	GetSkel()->AsyncSendPipeMessage(m, ee, false, boost::bind(&PipeMember_empty_handler,_1));
	}

	DeleteEndpoint(e);

	
	detail::PostHandler(node, handler, true);
	
}

void PipeServerBase::DeleteEndpoint(RR_SHARED_PTR<PipeEndpointBase> e)
{
	boost::mutex::scoped_lock lock(pipeendpoints_lock);
	//if (pipeendpoints.count(pipe_endpoint_server_id(e->GetEndpoint(),e->GetIndex()))==0) return;
	pipeendpoints.erase(pipe_endpoint_server_id(e->GetEndpoint(),e->GetIndex()));
}

void PipeServerBase::ClientDisconnected(RR_SHARED_PTR<ServerContext> context, ServerServiceListenerEventType ev, RR_SHARED_PTR<void> param)
{
	
	if (ev==ServerServiceListenerEventType_ClientDisconnected)
	{
		uint32_t ep=*RR_STATIC_POINTER_CAST<uint32_t>(param);
		std::vector<RR_SHARED_PTR<PipeEndpointBase> > p;

		{
			boost::mutex::scoped_lock lock(pipeendpoints_lock);
			for (RR_UNORDERED_MAP<pipe_endpoint_server_id, RR_SHARED_PTR<PipeEndpointBase> >::iterator ee= pipeendpoints.begin(); ee!=pipeendpoints.end(); )
			{
				if (ee->first.endpoint==ep)
				{
					//ee->second->RemoteClose();
					p.push_back(ee->second);

					ee=pipeendpoints.erase(ee);
				
				
				}
				else
				{
					++ee;
				}
			}
		}

		BOOST_FOREACH (RR_SHARED_PTR<PipeEndpointBase>& e, p)
		{
			try
			{
				e->Shutdown();
			}
			catch (std::exception& exp) {
				RobotRaconteurNode::TryHandleException(node, &exp);
			}
		}
		
	}
}

void pipe_server_client_disconnected(RR_SHARED_PTR<ServerContext> context, ServerServiceListenerEventType ev, RR_SHARED_PTR<void> param,RR_WEAK_PTR<PipeServerBase> w)
{
	RR_SHARED_PTR<PipeServerBase> p=w.lock();
	if (!p) return;
	p->ClientDisconnected(context,ev,param);

}


RR_SHARED_PTR<MessageEntry> PipeServerBase::PipeCommand(RR_SHARED_PTR<MessageEntry> m, uint32_t e)
{
	{
		boost::mutex::scoped_lock lock(pipeendpoints_lock);
		switch (m->EntryType)
		{
			case MessageEntryType_PipeConnectReq:
			{
					if (!init)
					{
						RR_WEAK_PTR<PipeServerBase> weak= RR_DYNAMIC_POINTER_CAST<PipeServerBase>(shared_from_this());
						listener_connection=GetSkel()->GetContext()->ServerServiceListener.connect(boost::bind(&pipe_server_client_disconnected,_1,_2,_3,weak));
						init=true;
					}
					
					int32_t index = RRArrayToScalar(m->FindElement("index")->CastData<RRArray<int32_t> >());
					
					
					BOOST_FOREACH (pipe_endpoint_server_id ee, pipeendpoints | boost::adaptors::map_keys)
					{
						if (ee.endpoint==e && ee.index >=index)
							index=ee.index+1;
					}
							
					if (index == -1)
					{
						index = 1;
						while (pipeendpoints.find(pipe_endpoint_server_id(e, index)) != pipeendpoints.end())
						{
							index++;
						}						
					}

					if (pipeendpoints.find(pipe_endpoint_server_id(e,index))!= pipeendpoints.end())
						throw InvalidArgumentException("Pipe endpoint index in use");

					bool isunreliable=false;

					RR_SHARED_PTR<MessageEntry> ret = RR_MAKE_SHARED<MessageEntry>(MessageEntryType_PipeConnectRet, GetMemberName());
					ret->AddElement("index", ScalarToRRArray(index));

					if (unreliable)
					{
						try
						{
							if (RRArrayToScalar((m->FindElement("unreliable")->CastData<RRArray<int32_t> >()))==1)
							{
								isunreliable=true;
								ret->AddElement("unreliable",ScalarToRRArray((int32_t)1));
							}
						}
						catch (std::exception&) {}
					
					
					}					
					
					RR_SHARED_PTR<PipeEndpointBase> p = CreateNewPipeEndpoint(index,e,isunreliable,direction,GetSkel()->GetContext()->UseMessage3(e));
					pipeendpoints.insert(std::make_pair(pipe_endpoint_server_id(e,index), p));

					lock.unlock();

					try
					{
						fire_PipeConnectCallback(p);
					}
					catch (std::exception& exp)
					{
						RobotRaconteurNode::TryHandleException(node, &exp);
					}

					
					return ret;
			}

			case MessageEntryType_PipeDisconnectReq:
			{
					
					int32_t index = RRArrayToScalar(m->FindElement("index")->CastData<RRArray<int32_t> >());
					
					RR_UNORDERED_MAP<pipe_endpoint_server_id, RR_SHARED_PTR<PipeEndpointBase> > ::iterator e1 = pipeendpoints.find(pipe_endpoint_server_id(e, index));
					if (e1 == pipeendpoints.end()) throw InvalidArgumentException("Invalid pipe");;
					RR_SHARED_PTR<PipeEndpointBase> ee = e1->second;					
					
					lock.unlock();
					ee->RemoteClose();
					//ep.erase(index);

					return RR_MAKE_SHARED<MessageEntry>(MessageEntryType_PipeDisconnectRet, GetMemberName());
			}
			default:
				throw InvalidOperationException("Invalid Command");

		}
	}
}

PipeServerBase::PipeServerBase(const std::string& name, RR_SHARED_PTR<ServiceSkel> skel, bool unreliable, MemberDefinition_Direction direction)
{
	m_MemberName=name;
	this->skel=skel;
	this->init=false;
	this->unreliable=unreliable;
	this->direction = direction;
	this->node=skel->RRGetNode();
	

}

RR_SHARED_PTR<ServiceSkel> PipeServerBase::GetSkel()
{
	RR_SHARED_PTR<ServiceSkel> out=skel.lock();
	if (!out) throw InvalidOperationException("Pipe has been closed");
	return out;
}


// PipeBroadcasterBase

namespace detail
{
	class PipeBroadcasterBase_connected_endpoint
	{
	public:
		PipeBroadcasterBase_connected_endpoint(RR_SHARED_PTR<PipeEndpointBase > ep)
		{
			//backlog=0;
			endpoint = ep;
			active_send_count = 0;
		}

		//Endpoints are stored within the pipe so it is safe to use weak_ptr here
		RR_WEAK_PTR<PipeEndpointBase > endpoint;
		std::list<uint32_t> backlog;
		std::list<uint32_t> forward_backlog;
		int32_t active_send_count;
		std::list<int32_t> active_sends;
	};

	struct PipeBroadcasterBase_async_send_operation
	{
		boost::mutex keys_lock;
		std::list<int32_t> keys;
	};
}

PipeBroadcasterBase::PipeBroadcasterBase()
{
	copy_element = false;
}

PipeBroadcasterBase::~PipeBroadcasterBase()
{

}

void PipeBroadcasterBase::InitBase(RR_SHARED_PTR<PipeBase> pipe, int32_t maximum_backlog)
{

	RR_SHARED_PTR<PipeServerBase> pipe1 = RR_DYNAMIC_POINTER_CAST<PipeServerBase>(pipe);
	if (!pipe1) throw InvalidArgumentException("Pipe must be a PipeServer for PipeBroadcaster");
	
	this->maximum_backlog = maximum_backlog;
	this->pipe = pipe1;
	this->node = pipe->GetNode();

	AttachPipeServerEvents(pipe1);
}

void PipeBroadcasterBase::EndpointConnectedBase(RR_SHARED_PTR<PipeEndpointBase > ep)
{
	boost::mutex::scoped_lock lock(endpoints_lock);

	if (maximum_backlog != 0)
	{
		ep->SetRequestPacketAck(true);
	}

	RR_SHARED_PTR<detail::PipeBroadcasterBase_connected_endpoint> cep = RR_MAKE_SHARED<detail::PipeBroadcasterBase_connected_endpoint>(ep);
	ep->SetIgnoreReceived(true);
	AttachPipeEndpointEvents(ep, cep);
	endpoints.push_back(cep);
}

void PipeBroadcasterBase::EndpointClosedBase(RR_SHARED_PTR<detail::PipeBroadcasterBase_connected_endpoint> ep)
{
	boost::mutex::scoped_lock lock(endpoints_lock);
	try
	{
		endpoints.remove(ep);
	}
	catch (std::exception&) {}
}

void PipeBroadcasterBase::PacketAckReceivedBase(RR_SHARED_PTR<detail::PipeBroadcasterBase_connected_endpoint> ep, uint32_t id)
{
	boost::mutex::scoped_lock lock(endpoints_lock);
	try
	{
		if (std::count(ep->backlog.begin(), ep->backlog.end(), id) == 0)
		{
			ep->forward_backlog.push_back(id);
		}
		else
		{
			ep->backlog.remove(id);
		}
	}
	catch (std::exception&) {}
}

void PipeBroadcasterBase::handle_send(int32_t id, RR_SHARED_PTR<RobotRaconteurException> err, RR_SHARED_PTR<detail::PipeBroadcasterBase_connected_endpoint> ep, RR_SHARED_PTR<detail::PipeBroadcasterBase_async_send_operation> op, int32_t key, int32_t send_key, boost::function<void()>& handler)
{

	{
		boost::mutex::scoped_lock lock(endpoints_lock);
		ep->active_sends.remove(send_key);

		if (maximum_backlog > -1)
		{

			if (std::count(ep->forward_backlog.begin(), ep->forward_backlog.end(), id) != 0)
			{
				ep->forward_backlog.remove(id);
			}
			else
			{
				ep->backlog.push_back(id);
			}
		}
	}

	{
		boost::mutex::scoped_lock lock(op->keys_lock);
		op->keys.remove(key);
		if (op->keys.size() != 0) return;
	}

	detail::InvokeHandler(node, handler);

}

void PipeBroadcasterBase::SendPacketBase(RR_SHARED_PTR<RRValue> packet)
{
	RR_SHARED_PTR<detail::sync_async_handler<void> > t = RR_MAKE_SHARED<detail::sync_async_handler<void> >();
	AsyncSendPacketBase(packet, boost::bind(&detail::sync_async_handler<void>::operator(), t));
	t->end_void();
}

void PipeBroadcasterBase::AsyncSendPacketBase(RR_SHARED_PTR<RRValue> packet, RR_MOVE_ARG(boost::function<void()>) handler)
{

	boost::mutex::scoped_lock lock(endpoints_lock);

	RR_SHARED_PTR<PipeBroadcasterBase> this_ = shared_from_this();

	RR_SHARED_PTR<detail::PipeBroadcasterBase_async_send_operation> op = RR_MAKE_SHARED<detail::PipeBroadcasterBase_async_send_operation>();
	boost::mutex::scoped_lock lock2(op->keys_lock);
	int32_t count = 0;

	for (std::list<RR_SHARED_PTR<detail::PipeBroadcasterBase_connected_endpoint> >::iterator ee = endpoints.begin(); ee != endpoints.end(); )
	{
		try
		{
			std::list<RR_SHARED_PTR<detail::PipeBroadcasterBase_connected_endpoint> >::iterator ee2 = ee;

			RR_SHARED_PTR<PipeEndpointBase> ep = (*ee)->endpoint.lock();
			if (!ep)
			{
				ee = endpoints.erase(ee);
				continue;
			}
			else
			{
				ee++;
			}

			if (predicate)
			{
				if (!predicate(this_, ep->GetEndpoint(), ep->GetIndex()))
					continue;
			}

			if (maximum_backlog > -1 && ((int32_t)(*ee2)->backlog.size()) + ((int32_t)(*ee2)->active_sends.size()) > maximum_backlog)
			{
				continue;
			}

			(*ee2)->active_send_count = (*ee2)->active_send_count < std::numeric_limits<int32_t>::max() ? (*ee2)->active_send_count + 1 : 0;
			int32_t send_key = (*ee2)->active_send_count;
			(*ee2)->active_sends.push_back(send_key);
			if (!copy_element)
			{
				ep->AsyncSendPacketBase(packet, boost::bind(&PipeBroadcasterBase::handle_send, this->shared_from_this(), _1, _2, *ee2, op, count, send_key, handler));
			}
			else
			{
				RR_SHARED_PTR<MessageElement> packet2 = ShallowCopyMessageElement(rr_cast<MessageElement>(packet));
				ep->AsyncSendPacketBase(packet2, boost::bind(&PipeBroadcasterBase::handle_send, this->shared_from_this(), _1, _2, *ee2, op, count, send_key, handler));
			}
			op->keys.push_back(count);


			count++;
		}
		catch (std::exception&) {}

	}

	if (op->keys.size() == 0)
	{
		detail::PostHandler(node, RR_MOVE(handler), true, true);
	}

}

size_t PipeBroadcasterBase::GetActivePipeEndpointCount()
{
	boost::mutex::scoped_lock lock(endpoints_lock);
	return endpoints.size();
}

void PipeBroadcasterBase::AttachPipeServerEvents(RR_SHARED_PTR<PipeServerBase> p)
{

}

void PipeBroadcasterBase::AttachPipeEndpointEvents(RR_SHARED_PTR<PipeEndpointBase> p, RR_SHARED_PTR<detail::PipeBroadcasterBase_connected_endpoint> cep)
{

}

boost::function<bool(RR_SHARED_PTR<PipeBroadcasterBase>&, uint32_t, int32_t) > PipeBroadcasterBase::GetPredicate()
{
	boost::mutex::scoped_lock lock(endpoints_lock);
	return predicate;
}

void PipeBroadcasterBase::SetPredicate(boost::function<bool(RR_SHARED_PTR<PipeBroadcasterBase>&, uint32_t, int32_t)> f)
{
	boost::mutex::scoped_lock lock(endpoints_lock);
	predicate = f;
}

RR_SHARED_PTR<PipeBase> PipeBroadcasterBase::GetPipeBase()
{
	RR_SHARED_PTR<PipeBase> pipe1 = pipe.lock();
	if (!pipe1) throw InvalidOperationException("Pipe released");
	return pipe1;
}


}
