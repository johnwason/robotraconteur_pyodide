﻿// Copyright 2011-2019 Wason Technology, LLC
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

#include "RobotRaconteur/Message.h"
#include "RobotRaconteur/Error.h"

#include "RobotRaconteur/IOUtils.h"
#include "RobotRaconteur/NodeID.h"
#include "RobotRaconteur/DataTypes.h"
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#include <boost/range.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/bind.hpp>

namespace RobotRaconteur
{

	Message::Message()
	{
		entries.clear();
	}	

	RR_INTRUSIVE_PTR<MessageEntry> Message::FindEntry(const std::string& name)
	{
		std::vector<RR_INTRUSIVE_PTR<MessageEntry> >::iterator m=boost::find_if(entries,
				boost::bind(&MessageEntry::MemberName, _1) == name);

		if (m==entries.end()) throw MessageEntryNotFoundException("Element " + name + " not found.");

		return *m;
	}

	RR_INTRUSIVE_PTR<MessageEntry> Message::AddEntry(MessageEntryType t, const std::string& name)
	{
		RR_INTRUSIVE_PTR<MessageEntry> m = CreateMessageEntry();
		m->MemberName = name;
		m->EntryType = t;
		
		entries.push_back(m);

		return m;
	}

	uint32_t Message::ComputeSize()
	{
		uint64_t s = header->ComputeSize();		
		BOOST_FOREACH (RR_INTRUSIVE_PTR<MessageEntry>& e, entries)
		{
			e->UpdateData();
			s += e->EntrySize;
		}

		if (s > std::numeric_limits<uint32_t>::max()) throw ProtocolException("Message exceeds maximum length");
		header->UpdateHeader(boost::numeric_cast<uint32_t>(s), boost::numeric_cast<uint16_t>(entries.size()));
		return boost::numeric_cast<uint32_t>(s);
	}

	void Message::Write(ArrayBinaryWriter &w)
	{

		uint32_t s = ComputeSize();

		w.PushRelativeLimit(s);

		header->UpdateHeader(s, boost::numeric_cast<uint16_t>(entries.size()));
		header->Write(w);
		BOOST_FOREACH (RR_INTRUSIVE_PTR<MessageEntry>& e, entries)
		{
			e->Write(w);
		}

		w.PopLimit();
		//if (w.DistanceFromLimit()!=0) throw ProtocolException("Error in message format");

	}

	void Message::Read( ArrayBinaryReader &r)
	{
		header = CreateMessageHeader();
		header->Read(r);

		r.PushRelativeLimit(header->MessageSize-header->HeaderSize);

		uint16_t s = header->EntryCount;
		entries.clear();
		for (int32_t i = 0; i < s; i++)
		{
			RR_INTRUSIVE_PTR<MessageEntry> e = CreateMessageEntry();
			e->Read(r);
			entries.push_back(e);
		}
	}

	uint32_t Message::ComputeSize3()
	{
		header->EntryCount = boost::numeric_cast<uint16_t>(entries.size());
		uint64_t s = 0;
		BOOST_FOREACH (RR_INTRUSIVE_PTR<MessageEntry>& e, entries)
		{
			e->UpdateData3();
			s += e->EntrySize;
		}

		if (s > std::numeric_limits<uint32_t>::max()) throw ProtocolException("Message exceeds maximum length");

		header->UpdateHeader3(boost::numeric_cast<uint32_t>(s), boost::numeric_cast<uint16_t>(entries.size()));

		uint32_t s1 = header->MessageSize;

		if (s1 > std::numeric_limits<uint32_t>::max()) throw ProtocolException("Message exceeds maximum length");
		return boost::numeric_cast<uint32_t>(s1);
	}

	void Message::Write3(ArrayBinaryWriter &w, const uint16_t& version_minor)
	{

		if (version_minor != 0) throw ProtocolException("Invalid Message 3 version minor");

		uint32_t s = ComputeSize3();

		w.PushRelativeLimit(s);
				
		header->Write3(w, version_minor);
		BOOST_FOREACH (RR_INTRUSIVE_PTR<MessageEntry>& e, entries)
		{
			e->Write3(w, version_minor);
		}

		w.PopLimit();
		//if (w.DistanceFromLimit()!=0) throw ProtocolException("Error in message format");

	}

	void Message::Read3(ArrayBinaryReader &r, uint16_t& version_minor)
	{
		header = CreateMessageHeader();
		header->Read3(r, version_minor);

		r.PushRelativeLimit(header->MessageSize - header->HeaderSize);

		uint16_t s = header->EntryCount;
		entries.clear();
		for (int32_t i = 0; i < s; i++)
		{
			RR_INTRUSIVE_PTR<MessageEntry> e = CreateMessageEntry();
			e->Read3(r, version_minor);
			entries.push_back(e);
		}
	}

	uint16_t MessageHeader::ComputeSize()
	{
		uint32_t s1=boost::numeric_cast<uint32_t>(ArrayBinaryWriter::GetStringByteCount8(SenderNodeName));
		uint32_t s2=boost::numeric_cast<uint32_t>(ArrayBinaryWriter::GetStringByteCount8(ReceiverNodeName));
		uint32_t s3=boost::numeric_cast<uint32_t>(ArrayBinaryWriter::GetStringByteCount8(MetaData));

		if (s1 > std::numeric_limits<uint16_t>::max()) throw ProtocolException("SenderNodeName exceeds maximum length");
		if (s2 > std::numeric_limits<uint16_t>::max()) throw ProtocolException("ReceiverNodeName exceeds maximum length");
		if (s3 > std::numeric_limits<uint16_t>::max()) throw ProtocolException("Header MetaData exceeds maximum length");
		
		uint32_t s=64 + s1 + s2 + s3;

		if (s > std::numeric_limits<uint16_t>::max()) throw ProtocolException("MessageHeader exceeds maximum length");

		return boost::numeric_cast<uint16_t>(s );
	}

	void MessageHeader::UpdateHeader(uint32_t message_size, uint16_t entry_count)
	{
		if (MessageFlags != MessageFlags_Version2Compat)
		{
			throw ProtocolException("Invalid message flags for Version 2 message");
		}

		HeaderSize = ComputeSize();
		MessageSize = message_size;
		EntryCount = entry_count;
	}

	void MessageHeader::Write( ArrayBinaryWriter &w)
	{
		w.PushRelativeLimit(HeaderSize);
		w.WriteString8("RRAC");
		w.WriteNumber(MessageSize);
		w.WriteNumber(boost::numeric_cast<uint16_t>(2));

		if (HeaderSize > std::numeric_limits<uint16_t>::max())
		{
			throw ProtocolException("Header too large for Message 2");
		}
		w.WriteNumber(boost::numeric_cast<uint16_t>(HeaderSize));

		const boost::array<uint8_t,16> bSenderNodeID = SenderNodeID.ToByteArray();
		const boost::array<uint8_t,16> bReceiverNodeID = ReceiverNodeID.ToByteArray();
		for (int32_t i = 0; i < 16; i++)
		{
			w.WriteNumber(bSenderNodeID[i]);
		};
		for (int32_t i = 0; i < 16; i++)
		{
			w.WriteNumber(bReceiverNodeID[i]);
		};
		w.WriteNumber(SenderEndpoint);
		w.WriteNumber(ReceiverEndpoint);
		w.WriteNumber(boost::numeric_cast<uint16_t>(ArrayBinaryWriter::GetStringByteCount8(SenderNodeName)));
		w.WriteString8(SenderNodeName);
		w.WriteNumber(boost::numeric_cast<uint16_t>(ArrayBinaryWriter::GetStringByteCount8(ReceiverNodeName)));
		w.WriteString8(ReceiverNodeName);
		w.WriteNumber(boost::numeric_cast<uint16_t>(ArrayBinaryWriter::GetStringByteCount8(MetaData)));
		w.WriteString8(MetaData);
		w.WriteNumber(boost::numeric_cast<uint16_t>(EntryCount));
		w.WriteNumber(MessageID);
		w.WriteNumber(MessageResID);
		
		if (w.DistanceFromLimit()!=0) throw DataSerializationException("Error in message format");
		w.PopLimit();
	}

	void MessageHeader::Read(ArrayBinaryReader &r)
	{
		std::string seed = r.ReadString8(4);
		if (seed != "RRAC")
			throw ProtocolException("Incorrect message seed");
		MessageSize = r.ReadNumber<uint32_t>();
		uint16_t version = r.ReadNumber<uint16_t>();
		if (version != 2)
			throw ProtocolException("Uknown protocol version");

		HeaderSize = r.ReadNumber<uint16_t>();

		r.PushRelativeLimit(HeaderSize-12);
		
		boost::array<uint8_t,16> bSenderNodeID;
		for (int32_t i = 0; i < 16; i++)
		{
			bSenderNodeID[i] = r.ReadNumber<uint8_t>();
		};
		SenderNodeID=NodeID(bSenderNodeID);
		
		boost::array<uint8_t,16> bReceiverNodeID;
		for (int32_t i = 0; i < 16; i++)
		{
			bReceiverNodeID[i] = r.ReadNumber<uint8_t>();
		};
		ReceiverNodeID = NodeID(bReceiverNodeID);
		SenderEndpoint = r.ReadNumber<uint32_t>();
		ReceiverEndpoint = r.ReadNumber<uint32_t>();
		uint16_t pname_s = r.ReadNumber<uint16_t>();
		SenderNodeName = r.ReadString8(pname_s);
		uint16_t pname_r = r.ReadNumber<uint16_t>();
		ReceiverNodeName = r.ReadString8(pname_r);
		uint16_t meta_s = r.ReadNumber<uint16_t>();
		MetaData = r.ReadString8(meta_s);

		EntryCount = r.ReadNumber<uint16_t>();
		MessageID = r.ReadNumber<uint16_t>();
		MessageResID = r.ReadNumber<uint16_t>();

		if (r.DistanceFromLimit()!=0) throw DataSerializationException("Error in message format");
		r.PopLimit();
	}

	uint32_t MessageHeader::ComputeSize3()
	{
		size_t s = 12;
		
		if (MessageFlags & MessageFlags_PROTOCOL_VERSION_MINOR)
		{
			s += 2;
		}

		if (MessageFlags & MessageFlags_SUBSTREAM_ID)
		{
			s += ArrayBinaryWriter::GetUintX2ByteCount(SubstreamID);
		}

		if (MessageFlags & MessageFlags_SUBSTREAM_SEQUENCE_NUMBER)
		{
			s += ArrayBinaryWriter::GetUintXByteCount(SubstreamSequenceNumber.SequenceNumber);
			s += ArrayBinaryWriter::GetUintXByteCount(SubstreamSequenceNumber.RecvSequenceNumber);
		}

		if (MessageFlags & MessageFlags_FRAGMENT)
		{
			s += ArrayBinaryWriter::GetUintXByteCount(FragmentHeader.FragmentMessageNumber);
			s += ArrayBinaryWriter::GetUintXByteCount(FragmentHeader.FragmentMessageSize);
			s += ArrayBinaryWriter::GetUintXByteCount(FragmentHeader.FragmentOffset);
		}

		if (MessageFlags & MessageFlags_UNRELIABLE_EXPIRATION)
		{
			s += ArrayBinaryWriter::GetIntX2ByteCount(UnreliableExpiration.seconds);
			s += ArrayBinaryWriter::GetIntXByteCount(UnreliableExpiration.nanoseconds);
		}

		if (MessageFlags & MessageFlags_PRIORITY)
		{
			s += 2;
		}

		if (MessageFlags & MessageFlags_ROUTING_INFO)
		{
			s += 32;
			s+=ArrayBinaryWriter::GetStringByteCount8WithXLen(SenderNodeName);
			s+=ArrayBinaryWriter::GetStringByteCount8WithXLen(ReceiverNodeName);			
		}

		if (MessageFlags & MessageFlags_ENDPOINT_INFO)
		{
			s += 8;
		}

		if (MessageFlags & MessageFlags_META_INFO)
		{
			s += ArrayBinaryWriter::GetStringByteCount8WithXLen(MetaData);
		}
		
		if (MessageFlags & MessageFlags_MESSAGE_ID)
		{
			s += 4;
		}

		if (MessageFlags & MessageFlags_STRING_TABLE)
		{
			uint32_t s1 = 0;
			for (std::vector < boost::tuple<uint32_t, std::string> >::iterator e = StringTable.begin(); e != StringTable.end(); e++)
			{
				s1 += ArrayBinaryWriter::GetUintXByteCount(e->get<0>());
				s1 += ArrayBinaryWriter::GetStringByteCount8WithXLen(e->get<1>());
			}
			if (s1 > 1024) throw ProtocolException("String table too large");
			s += s1 + ArrayBinaryWriter::GetUintXByteCount(StringTable.size());
		}

		if (MessageFlags & MessageFlags_MULTIPLE_ENTRIES)
		{
			s += ArrayBinaryWriter::GetUintXByteCount(EntryCount);
		}

		if (MessageFlags & MessageFlags_TRANSPORT_SPECIFIC)
		{
			s += TransportSpecific.size();
			s += ArrayBinaryWriter::GetUintXByteCount(TransportSpecific.size());
		}

		s = ArrayBinaryWriter::GetSizePlusUintX(s);

		if (s > std::numeric_limits<uint32_t>::max()) throw ProtocolException("MessageHeader exceeds maximum length");

		return boost::numeric_cast<uint32_t>(s);
	}

	void MessageHeader::UpdateHeader3(uint32_t message_entry_size, uint16_t entry_count)
	{
		if (entry_count == 1)
		{
			MessageFlags &= ~MessageFlags_MULTIPLE_ENTRIES;
		}
		else
		{
			MessageFlags |= MessageFlags_MULTIPLE_ENTRIES;
		}

		if (MetaData.size() == 0)
		{
			MessageFlags &= ~MessageFlags_META_INFO;
		}
		else
		{
			MessageFlags |= MessageFlags_META_INFO;
		}

		EntryCount = entry_count;
		HeaderSize = ComputeSize3();
		MessageSize = message_entry_size + HeaderSize;
		
	}

	void MessageHeader::Write3(ArrayBinaryWriter &w, const uint16_t& version_minor)
	{
		if (version_minor != 0) throw ProtocolException("Invalid Message 3 version minor");

		w.PushRelativeLimit(HeaderSize);
		w.WriteString8("RRAC");
		w.WriteNumber(MessageSize);
		w.WriteNumber(boost::numeric_cast<uint16_t>(3));

		w.WriteUintX(HeaderSize);
		w.WriteNumber(MessageFlags);

		if (MessageFlags & MessageFlags_PROTOCOL_VERSION_MINOR)
		{			
			w.WriteNumber(version_minor);
		}

		if (MessageFlags & MessageFlags_SUBSTREAM_ID)
		{
			w.WriteUintX2(SubstreamID);
		}

		if (MessageFlags & MessageFlags_SUBSTREAM_SEQUENCE_NUMBER)
		{
			w.WriteUintX(SubstreamSequenceNumber.SequenceNumber);
			w.WriteUintX(SubstreamSequenceNumber.RecvSequenceNumber);
		}

		if (MessageFlags & MessageFlags_FRAGMENT)
		{
			w.WriteUintX(FragmentHeader.FragmentMessageNumber);
			w.WriteUintX(FragmentHeader.FragmentMessageSize);
			w.WriteUintX(FragmentHeader.FragmentOffset);
		}

		if (MessageFlags & MessageFlags_UNRELIABLE_EXPIRATION)
		{
			w.WriteIntX2(UnreliableExpiration.seconds);
			w.WriteIntX(UnreliableExpiration.nanoseconds);
		}

		if (MessageFlags & MessageFlags_PRIORITY)
		{
			w.WriteNumber(Priority);
		}

		if (MessageFlags & MessageFlags_ROUTING_INFO)
		{

			const boost::array<uint8_t, 16> bSenderNodeID = SenderNodeID.ToByteArray();
			const boost::array<uint8_t, 16> bReceiverNodeID = ReceiverNodeID.ToByteArray();
			for (int32_t i = 0; i < 16; i++)
			{
				w.WriteNumber(bSenderNodeID[i]);
			};
			for (int32_t i = 0; i < 16; i++)
			{
				w.WriteNumber(bReceiverNodeID[i]);
			};
			
			w.WriteString8WithXLen(SenderNodeName);			
			w.WriteString8WithXLen(ReceiverNodeName);
		}

		if (MessageFlags & MessageFlags_ENDPOINT_INFO)
		{
			w.WriteNumber(SenderEndpoint);
			w.WriteNumber(ReceiverEndpoint);
		}

		if (MessageFlags & MessageFlags_META_INFO)
		{
			w.WriteString8WithXLen(MetaData);
		}
		
		if (MessageFlags & MessageFlags_MESSAGE_ID)
		{			
			w.WriteNumber(MessageID);
			w.WriteNumber(MessageResID);
		}

		if (MessageFlags & MessageFlags_STRING_TABLE)
		{
			w.WriteUintX(StringTable.size());
			for (std::vector < boost::tuple<uint32_t, std::string> >::iterator e = StringTable.begin(); e != StringTable.end(); e++)
			{
				w.WriteUintX(e->get<0>());
				w.WriteString8WithXLen(e->get<1>());
			}
		}
		
		if (MessageFlags & MessageFlags_MULTIPLE_ENTRIES)
		{
			w.WriteUintX(EntryCount);
		}

		if (MessageFlags & MessageFlags_TRANSPORT_SPECIFIC)
		{
			w.WriteUintX(TransportSpecific.size());
			if (!TransportSpecific.empty())
			{
				w.Write(&TransportSpecific[0], 0, TransportSpecific.size());
			}
		}

		if (w.DistanceFromLimit() != 0) throw DataSerializationException("Error in message format");
		w.PopLimit();

	}

	void MessageHeader::Read3(ArrayBinaryReader &r, uint16_t& version_minor)
	{
		std::string magic = r.ReadString8(4);
		if (magic != "RRAC")
			throw ProtocolException("Incorrect message magic");
		MessageSize = r.ReadNumber<uint32_t>();
		uint16_t version = r.ReadNumber<uint16_t>();
		if (version != 3)
			throw ProtocolException("Unknown protocol version");

		HeaderSize = r.ReadUintX();

		r.PushRelativeLimit(HeaderSize - 10 - ArrayBinaryWriter::GetUintXByteCount(HeaderSize));

		MessageFlags = r.ReadNumber<uint16_t>();

		if (MessageFlags & MessageFlags_PROTOCOL_VERSION_MINOR)
		{
			version_minor = r.ReadNumber<uint16_t>();
			
			if (version_minor != 0) throw ProtocolException("Invalid Message 3 version minor");			
		}
		else
		{
			version_minor = 0;
		}

		if (MessageFlags & MessageFlags_SUBSTREAM_ID)
		{
			SubstreamID = r.ReadUintX2();
		}

		if (MessageFlags & MessageFlags_SUBSTREAM_SEQUENCE_NUMBER)
		{
			SubstreamSequenceNumber.SequenceNumber = r.ReadUintX();
			SubstreamSequenceNumber.RecvSequenceNumber = r.ReadUintX();
		}

		if (MessageFlags & MessageFlags_FRAGMENT)
		{
			FragmentHeader.FragmentMessageNumber = r.ReadUintX();
			FragmentHeader.FragmentMessageSize = r.ReadUintX();
			FragmentHeader.FragmentOffset = r.ReadUintX();
		}

		if (MessageFlags & MessageFlags_UNRELIABLE_EXPIRATION)
		{
			UnreliableExpiration.seconds = r.ReadIntX2();
			UnreliableExpiration.nanoseconds = r.ReadIntX();
		}

		if (MessageFlags & MessageFlags_PRIORITY)
		{
			Priority = r.ReadNumber<uint16_t>();
		}

		if (MessageFlags & MessageFlags_ROUTING_INFO)
		{

			boost::array<uint8_t, 16> bSenderNodeID;
			for (int32_t i = 0; i < 16; i++)
			{
				bSenderNodeID[i] = r.ReadNumber<uint8_t>();
			};
			SenderNodeID = NodeID(bSenderNodeID);

			boost::array<uint8_t, 16> bReceiverNodeID;
			for (int32_t i = 0; i < 16; i++)
			{
				bReceiverNodeID[i] = r.ReadNumber<uint8_t>();
			};
			ReceiverNodeID = NodeID(bReceiverNodeID);
			
			uint32_t pname_s = r.ReadUintX();
			SenderNodeName = r.ReadString8(pname_s);
			uint32_t pname_r = r.ReadUintX();
			ReceiverNodeName = r.ReadString8(pname_r);
		}

		if (MessageFlags & MessageFlags_ENDPOINT_INFO)
		{
			SenderEndpoint = r.ReadNumber<uint32_t>();
			ReceiverEndpoint = r.ReadNumber<uint32_t>();
		}

		if (MessageFlags & MessageFlags_META_INFO)
		{
			uint32_t meta_s = r.ReadUintX();
			MetaData = r.ReadString8(meta_s);
		}
		
		if (MessageFlags & MessageFlags_MESSAGE_ID)
		{
			MessageID = r.ReadNumber<uint16_t>();
			MessageResID = r.ReadNumber<uint16_t>();
		}

		if (MessageFlags & MessageFlags_STRING_TABLE)
		{
			uint32_t s1 = r.ReadUintX();
			for (uint32_t i = 0; i < s1; i++)
			{
				uint32_t c = r.ReadUintX();
				uint32_t l = r.ReadUintX();
				std::string v = r.ReadString8(l);
				StringTable.push_back(boost::make_tuple(c, v));
			}
		}

		if (MessageFlags & MessageFlags_MULTIPLE_ENTRIES)
		{
			uint32_t c = r.ReadUintX();
			if (c > std::numeric_limits<uint16_t>::max()) throw ProtocolException("Too many entries in message");
			EntryCount = boost::numeric_cast<uint16_t>(c);
		}
		else
		{
			EntryCount = 1;
		}

		if (MessageFlags & MessageFlags_TRANSPORT_SPECIFIC)
		{
			size_t l = r.ReadUintX();
			TransportSpecific.resize(l);
			if (l != 0)
			{
				r.Read(&TransportSpecific[0], 0, l);
			}
		}

		if (r.DistanceFromLimit() != 0) throw DataSerializationException("Error in message format");
		r.PopLimit();
	}

	MessageHeader::MessageHeader()
	{
		SenderNodeName = "";
		ReceiverNodeName = "";
		
		MetaData = "";
		ReceiverNodeID=NodeID();
		SenderNodeID=NodeID();

		MessageSize=0;
		HeaderSize=0;
		SenderEndpoint=0;
		ReceiverEndpoint=0;
		MessageID=0;
		MessageResID=0;
		EntryCount=0;

		memset(&FragmentHeader, 0, sizeof(FragmentHeader));

		SubstreamID = 0;
		MessageFlags = MessageFlags_Version2Compat;
		Priority = 0;
	}

	MessageEntry::MessageEntry()
	{
		ServicePathCode = 0;
		MemberNameCode = 0;
		RequestID = 0;
		Error = MessageErrorType_None;
		EntryType = MessageEntryType_Null;
		EntrySize = 0;
		EntryFlags = MessageEntryFlags_Version2Compat;
		EntryStreamID = 0;
		elements.clear();
		
	}

	MessageEntry::MessageEntry(MessageEntryType t, const std::string& n)
	{
		ServicePathCode = 0;
		MemberNameCode = 0;
		RequestID = 0;
		Error = MessageErrorType_None;
		EntryType = MessageEntryType_Null;
		EntrySize = 0;
		EntryFlags = MessageEntryFlags_Version2Compat;
		EntryStreamID = 0;
		elements.clear();
		EntryType = t;
		MemberName = n;
	}

	uint32_t MessageEntry::ComputeSize()
	{
		uint64_t s = 22;
		BOOST_FOREACH (RR_INTRUSIVE_PTR<MessageElement>& e, elements)
		{
			e->UpdateData();
			s += e->ElementSize;
		}

		uint32_t s1 = boost::numeric_cast<uint32_t>(ArrayBinaryWriter::GetStringByteCount8(ServicePath));
		uint32_t s2 = boost::numeric_cast<uint32_t>(ArrayBinaryWriter::GetStringByteCount8(MemberName));
		uint32_t s3 = boost::numeric_cast<uint32_t>(ArrayBinaryWriter::GetStringByteCount8(MetaData));

		if (s1 > std::numeric_limits<uint16_t>::max()) throw ProtocolException("ServicePath exceeds maximum length");
		if (s2 > std::numeric_limits<uint16_t>::max()) throw ProtocolException("MemberName exceeds maximum length");
		if (s3 > std::numeric_limits<uint16_t>::max()) throw ProtocolException("MessageEntry MetaData exceeds maximum length");

		s+= s1 + s2 + s3;

		if (s > std::numeric_limits<uint32_t>::max()) throw ProtocolException("MessageEntry exceeds maximum length");

		return boost::numeric_cast<uint32_t>(s);
	}

	RR_INTRUSIVE_PTR<MessageElement> MessageEntry::FindElement(const std::string& name)
	{
		std::vector<RR_INTRUSIVE_PTR<MessageElement> >::iterator m=boost::find_if(elements,
				boost::bind(&MessageElement::ElementName, _1) == name);

		if (m==elements.end()) throw MessageElementNotFoundException("Element " + name + " not found.");

		return *m;
	}

	bool MessageEntry::TryFindElement(const std::string& name, RR_INTRUSIVE_PTR<MessageElement>& elem)
	{
		std::vector<RR_INTRUSIVE_PTR<MessageElement> >::iterator m = boost::find_if(elements,
			boost::bind(&MessageElement::ElementName, _1) == name);

		if (m == elements.end()) return false;

		elem = *m;
		return true;
	}

	RR_INTRUSIVE_PTR<MessageElement> MessageEntry::AddElement(const std::string& name, RR_INTRUSIVE_PTR<MessageElementData> data)
	{
		RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
		m->ElementName = name;
		m->SetData(data);
		
		elements.push_back(m);

		return m;
	}

	RR_INTRUSIVE_PTR<MessageElement> MessageEntry::AddElement(RR_INTRUSIVE_PTR<MessageElement> m)
	{
		
		elements.push_back(m);

		return m;
	}

	void MessageEntry::UpdateData()
	{
		if (EntryFlags != MessageEntryFlags_Version2Compat)
		{
			throw ProtocolException("Invalid message flags for Version 2 message");
		}

		EntrySize = ComputeSize();
	}

	void MessageEntry::Write(ArrayBinaryWriter &w)
	{
		UpdateData();		
		
		w.PushRelativeLimit(EntrySize);
		
		w.WriteNumber(EntrySize);
		w.WriteNumber(static_cast<uint16_t>(EntryType));
		w.WriteNumber(static_cast<uint16_t>(0));

		w.WriteNumber(boost::numeric_cast<uint16_t>(ArrayBinaryWriter::GetStringByteCount8(ServicePath)));
		w.WriteString8(ServicePath);
		w.WriteNumber(boost::numeric_cast<uint16_t>(ArrayBinaryWriter::GetStringByteCount8(MemberName)));
		w.WriteString8(MemberName);
		w.WriteNumber(RequestID);
		w.WriteNumber(static_cast<uint16_t>(Error));
		w.WriteNumber(boost::numeric_cast<uint16_t>(ArrayBinaryWriter::GetStringByteCount8(MetaData)));
		w.WriteString8(MetaData);
		w.WriteNumber(boost::numeric_cast<uint16_t>(elements.size()));

		BOOST_FOREACH (RR_INTRUSIVE_PTR<MessageElement>& e, elements)
		{
			e->Write(w);
		}

		if (w.DistanceFromLimit()!=0) throw DataSerializationException("Error in message format");
		w.PopLimit();
		
	}

	void MessageEntry::Read(ArrayBinaryReader &r)
	{
		EntrySize = r.ReadNumber<uint32_t>();

		r.PushRelativeLimit(EntrySize-4);

		EntryType = boost::numeric_cast<MessageEntryType>(r.ReadNumber<uint16_t>());
		r.ReadNumber<uint16_t>();

		uint16_t sname_s = r.ReadNumber<uint16_t>();
		ServicePath = r.ReadString8(sname_s);
		uint16_t mname_s = r.ReadNumber<uint16_t>();
		MemberName = r.ReadString8(mname_s);
		RequestID = r.ReadNumber<uint32_t>();
		Error = boost::numeric_cast<MessageErrorType>(r.ReadNumber<uint16_t>());

		uint16_t metadata_s = r.ReadNumber<uint16_t>();
		MetaData = r.ReadString8(metadata_s);

		uint16_t ecount = r.ReadNumber<uint16_t>();



		elements = std::vector<RR_INTRUSIVE_PTR<MessageElement> >();
		for (int32_t i = 0; i < ecount; i++)
		{
			RR_INTRUSIVE_PTR<MessageElement> e = CreateMessageElement();
			e->Read(r);
			elements.push_back(e);
		}

		if (r.DistanceFromLimit()!=0) throw DataSerializationException("Error in message format");
		r.PopLimit();
		

	}

	uint32_t MessageEntry::ComputeSize3()
	{
		size_t s = 3;
		BOOST_FOREACH (RR_INTRUSIVE_PTR<MessageElement>& e, elements)
		{
			e->UpdateData3();
			s += e->ElementSize;
		}

		bool send_streamid = true;

		if (EntryFlags & MessageEntryFlags_SERVICE_PATH_STR)
		{
			s += boost::numeric_cast<uint32_t>(ArrayBinaryWriter::GetStringByteCount8WithXLen(ServicePath));
			send_streamid = false;
		}
		if (EntryFlags & MessageEntryFlags_SERVICE_PATH_CODE)
		{
			s += ArrayBinaryWriter::GetUintXByteCount(ServicePathCode);
			send_streamid = false;
		}

		if (EntryFlags & MessageEntryFlags_MEMBER_NAME_STR)
		{
			s += boost::numeric_cast<uint32_t>(ArrayBinaryWriter::GetStringByteCount8WithXLen(MemberName));
			send_streamid = false;
		}
		if (EntryFlags & MessageEntryFlags_MEMBER_NAME_CODE)
		{
			s += ArrayBinaryWriter::GetUintXByteCount(MemberNameCode);
			send_streamid = false;
		}

		if (send_streamid)
		{
			s += ArrayBinaryWriter::GetUintX2ByteCount(EntryStreamID);
		}

		if (EntryFlags & MessageEntryFlags_REQUEST_ID)
		{
			s += ArrayBinaryWriter::GetUintXByteCount(RequestID);
		}

		if (EntryFlags & MessageEntryFlags_ERROR)
		{
			s += 2;
		}

		if (EntryFlags & MessageEntryFlags_TIMESPEC)
		{
			s += ArrayBinaryWriter::GetIntX2ByteCount(EntryTimeSpec.seconds) + ArrayBinaryWriter::GetIntXByteCount(EntryTimeSpec.nanoseconds);
		}

		if (EntryFlags & MessageEntryFlags_META_INFO)
		{
			s += boost::numeric_cast<uint32_t>(ArrayBinaryWriter::GetStringByteCount8WithXLen(MetaData));
		}		

		s += ArrayBinaryWriter::GetUintXByteCount(elements.size());

		s = ArrayBinaryWriter::GetSizePlusUintX(s);

		return boost::numeric_cast<uint32_t>(s);
	}

	void MessageEntry::UpdateData3()
	{	

		if (RequestID != 0)
		{
			EntryFlags |= MessageEntryFlags_REQUEST_ID;
		}
		else
		{
			EntryFlags &= ~MessageEntryFlags_REQUEST_ID;
		}

		if (Error != 0)
		{
			EntryFlags |= MessageEntryFlags_ERROR;
		}
		else
		{
			EntryFlags &= ~MessageEntryFlags_ERROR;
		}

		if (MetaData.size() > 0)
		{
			EntryFlags |= MessageEntryFlags_META_INFO;
		}
		else
		{
			EntryFlags &= ~MessageEntryFlags_META_INFO;
		}

		EntrySize = ComputeSize3();
	}

	void MessageEntry::Write3(ArrayBinaryWriter &w, const uint16_t& version_minor)
	{
		if (version_minor != 0) throw ProtocolException("Invalid Message 3 version minor");

		UpdateData3();

		w.PushRelativeLimit(EntrySize);

		w.WriteUintX(EntrySize);
		w.WriteNumber(EntryFlags);
		w.WriteNumber(static_cast<uint16_t>(EntryType));
		
		bool send_streamid = true;

		if (EntryFlags & MessageEntryFlags_SERVICE_PATH_STR)
		{
			w.WriteString8WithXLen(ServicePath);
			send_streamid = false;
		}

		if (EntryFlags & MessageEntryFlags_SERVICE_PATH_CODE)
		{
			send_streamid = false;
			w.WriteUintX(ServicePathCode);
		}		

		if (EntryFlags & MessageEntryFlags_MEMBER_NAME_STR)
		{
			w.WriteString8WithXLen(MemberName);
			send_streamid = false;
		}

		if (EntryFlags & MessageEntryFlags_MEMBER_NAME_CODE)
		{
			send_streamid = false;
			w.WriteUintX(MemberNameCode);
		}
		
		if (send_streamid)
		{
			w.WriteUintX2(EntryStreamID);
		}

		if (EntryFlags & MessageEntryFlags_REQUEST_ID)
		{
			w.WriteUintX(RequestID);
		}

		if (EntryFlags & MessageEntryFlags_ERROR)
		{
			w.WriteNumber(static_cast<uint16_t>(Error));
		}

		if (EntryFlags & MessageEntryFlags_META_INFO)
		{
			w.WriteString8WithXLen(MetaData);
		}		

		if (EntryFlags & MessageEntryFlags_TIMESPEC)
		{
			w.WriteIntX2(EntryTimeSpec.seconds);
			w.WriteIntX(EntryTimeSpec.nanoseconds);
		}

		w.WriteUintX(boost::numeric_cast<uint32_t>(elements.size()));

		BOOST_FOREACH (RR_INTRUSIVE_PTR<MessageElement>& e, elements)
		{
			e->Write3(w, version_minor);
		}

		if (w.DistanceFromLimit() != 0) throw DataSerializationException("Error in message format");
		w.PopLimit();

	}

	void MessageEntry::Read3(ArrayBinaryReader &r, const uint16_t& version_minor)
	{
		if (version_minor != 0) throw ProtocolException("Invalid Message 3 version minor");

		EntrySize = r.ReadUintX();

		r.PushRelativeLimit(EntrySize - ArrayBinaryWriter::GetUintXByteCount(EntrySize));

		EntryFlags = r.ReadNumber<uint8_t>();
		EntryType = boost::numeric_cast<MessageEntryType>(r.ReadNumber<uint16_t>());
		
		bool read_streamid = true;

		if (EntryFlags & MessageEntryFlags_SERVICE_PATH_STR)
		{
			read_streamid = false;
			uint32_t sname_s = r.ReadUintX();
			ServicePath = r.ReadString8(sname_s);
		}

		if (EntryFlags & MessageEntryFlags_SERVICE_PATH_CODE)
		{
			read_streamid = false;
			ServicePathCode = r.ReadUintX();
		}
		
		if (EntryFlags & MessageEntryFlags_MEMBER_NAME_STR)
		{
			uint32_t mname_s = r.ReadUintX();
			MemberName = r.ReadString8(mname_s);
			read_streamid = false;
		}

		if (EntryFlags & MessageEntryFlags_MEMBER_NAME_CODE)
		{
			read_streamid = false;
			MemberNameCode = r.ReadUintX();
		}

		if (read_streamid)
		{
			EntryStreamID = r.ReadUintX2();
		}

		if (EntryFlags & MessageEntryFlags_REQUEST_ID)
		{
			RequestID = r.ReadUintX();
		}

		if (EntryFlags & MessageEntryFlags_ERROR)
		{
			Error = boost::numeric_cast<MessageErrorType>(r.ReadNumber<uint16_t>());
		}

		if (EntryFlags & MessageEntryFlags_META_INFO)
		{
			uint32_t metadata_s = r.ReadUintX();
			MetaData = r.ReadString8(metadata_s);
		}

		if (EntryFlags & MessageEntryFlags_TIMESPEC)
		{
			EntryTimeSpec.seconds = r.ReadIntX2();
			EntryTimeSpec.nanoseconds = r.ReadIntX();
		}

		uint32_t ecount = r.ReadUintX();


		elements = std::vector<RR_INTRUSIVE_PTR<MessageElement> >();
		for (int32_t i = 0; i < ecount; i++)
		{
			RR_INTRUSIVE_PTR<MessageElement> e = CreateMessageElement();
			e->Read3(r, version_minor);
			elements.push_back(e);
		}

		if (r.DistanceFromLimit() != 0) throw DataSerializationException("Error in message format");
		r.PopLimit();
	}

	MessageElement::MessageElement()
	{
		ElementSize = 0;
		DataCount = 0;

		ElementFlags = MessageElementFlags_Version2Compat;
		ElementNameCode = 0;
		ElementNumber = 0;
		ElementTypeNameCode = 0;

		ElementType = DataTypes_void_t;
		SequenceNumber = 0;
	}

	MessageElement::MessageElement(const std::string& name, RR_INTRUSIVE_PTR<MessageElementData> datin)
	{
		ElementSize = 0;
		DataCount = 0;

		ElementFlags = MessageElementFlags_Version2Compat;
		ElementNameCode = 0;
		ElementNumber = 0;
		ElementTypeNameCode = 0;

		ElementType = DataTypes_void_t;

		ElementName = name;
		SetData(datin);
		//UpdateData();
	}

	RR_INTRUSIVE_PTR<MessageElementData> MessageElement::GetData() 
	{
		return dat;
	}

	void MessageElement::SetData(const RR_INTRUSIVE_PTR<MessageElementData> value)
	{
		dat = value;

		if (value)
		{
			ElementType = value->GetTypeID();
		}
		else
		{
			ElementType = DataTypes_void_t;
		}

		ElementSize = std::numeric_limits<uint32_t>::max();
		//UpdateData();
	}

	uint32_t MessageElement::ComputeSize()
	{
		uint64_t s = 16;
		uint32_t s1= boost::numeric_cast<uint32_t>(ArrayBinaryWriter::GetStringByteCount8(ElementName));
		uint32_t s2=boost::numeric_cast<uint32_t>(ArrayBinaryWriter::GetStringByteCount8(ElementTypeName));
		uint32_t s3=boost::numeric_cast<uint32_t>(ArrayBinaryWriter::GetStringByteCount8(MetaData));

		if (s1 > std::numeric_limits<uint16_t>::max()) throw ProtocolException("ElementName exceeds maximum length");
		if (s2 > std::numeric_limits<uint16_t>::max()) throw ProtocolException("ElementTypeName exceeds maximum length");
		if (s3 > std::numeric_limits<uint16_t>::max()) throw ProtocolException("MessageElement MetaData exceeds maximum length");
		
		s+=s1+s2+s3;
		
		switch (ElementType)
		{
		case DataTypes_void_t:			
			break;
		case DataTypes_double_t:
		case DataTypes_single_t:
		case DataTypes_int8_t:
		case DataTypes_uint8_t:
		case DataTypes_int16_t:
		case DataTypes_uint16_t:
		case DataTypes_int32_t:
		case DataTypes_uint32_t:
		case DataTypes_int64_t:
		case DataTypes_uint64_t:
		case DataTypes_string_t:
		case DataTypes_cdouble_t:
		case DataTypes_csingle_t:
		case DataTypes_bool_t:
			s += DataCount * RRArrayElementSize(ElementType);
			break;		
		case DataTypes_structure_t:
		{
			RR_INTRUSIVE_PTR<MessageElementStructure> d = rr_cast<MessageElementStructure>(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_vector_t:
		{
			RR_INTRUSIVE_PTR<MessageElementMap<int32_t> > d = rr_cast<MessageElementMap<int32_t> >(GetData());

			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_dictionary_t:
		{
			RR_INTRUSIVE_PTR<MessageElementMap<std::string> > d = rr_cast<MessageElementMap<std::string> >(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementMultiDimArray> d = rr_cast<MessageElementMultiDimArray>(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_list_t:
		{
			RR_INTRUSIVE_PTR<MessageElementList > d = rr_cast<MessageElementList >(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_pod_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPod > d = rr_cast<MessageElementPod >(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_pod_array_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPodArray > d = rr_cast<MessageElementPodArray >(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_pod_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPodMultiDimArray > d = rr_cast<MessageElementPodMultiDimArray >(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData();
				s += e->ElementSize;
			}
			break;
		}		
		case DataTypes_namedarray_array_t:
		{
			RR_INTRUSIVE_PTR<MessageElementNamedArray > d = rr_cast<MessageElementNamedArray>(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_namedarray_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementNamedMultiDimArray > d = rr_cast<MessageElementNamedMultiDimArray>(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData();
				s += e->ElementSize;
			}
			break;
		}
		default:
			throw DataTypeException("Unknown data type");
		}
				
		if (s > std::numeric_limits<uint32_t>::max()) throw ProtocolException("MessageElement exceeds maximum length");

		return boost::numeric_cast<uint32_t>(s);
	}

	void MessageElement::UpdateData()
	{
		if (ElementFlags & MessageElementFlags_ELEMENT_NUMBER && !(ElementFlags & MessageElementFlags_ELEMENT_NAME_STR))
		{
			ElementName = boost::lexical_cast<std::string>(ElementNumber);
			ElementFlags &= ~MessageElementFlags_ELEMENT_NUMBER;
			ElementFlags |= MessageElementFlags_ELEMENT_NAME_STR;
		}

		if (ElementFlags != MessageElementFlags_Version2Compat)
		{
			throw ProtocolException("Invalid message flags for Version 2 message");
		}

		std::string datatype;
		if (!dat)
			ElementType=DataTypes_void_t;
		else
			ElementType=dat->GetTypeID();

		ElementTypeName="";
		switch (ElementType)
		{
		case DataTypes_void_t:
			DataCount=0;
			break;
		case DataTypes_double_t:
		case DataTypes_single_t:
		case DataTypes_int8_t:
		case DataTypes_uint8_t:
		case DataTypes_int16_t:
		case DataTypes_uint16_t:
		case DataTypes_int32_t:
		case DataTypes_uint32_t:
		case DataTypes_int64_t:
		case DataTypes_uint64_t:
		case DataTypes_string_t:
		case DataTypes_cdouble_t:
		case DataTypes_csingle_t:
		case DataTypes_bool_t:
			{
				RR_INTRUSIVE_PTR<RRBaseArray> rdat = RR_DYNAMIC_POINTER_CAST<RRBaseArray>(dat);
				if (!rdat) throw DataTypeException("");
				DataCount= boost::numeric_cast<uint32_t>(rdat->size());
				break;
			}
		case DataTypes_structure_t:
			{
				RR_INTRUSIVE_PTR<MessageElementStructure> sdat = RR_DYNAMIC_POINTER_CAST<MessageElementStructure>(dat);
				if (!sdat) throw DataTypeException("");
				DataCount = boost::numeric_cast<uint32_t>(sdat->Elements.size());
				ElementTypeName=sdat->GetTypeString();
				break;
			}
		case DataTypes_vector_t:
			{
				RR_INTRUSIVE_PTR<MessageElementMap<int32_t> > vdat = RR_DYNAMIC_POINTER_CAST<MessageElementMap<int32_t> >(dat);
				if (!vdat) throw DataTypeException("");
				DataCount = boost::numeric_cast<uint32_t>(vdat->Elements.size());
				break;
			}
		case DataTypes_dictionary_t:
			{
				RR_INTRUSIVE_PTR<MessageElementMap<std::string> > ddat = RR_STATIC_POINTER_CAST<MessageElementMap<std::string> >(dat);
				if (!ddat) throw DataTypeException("");
				DataCount = boost::numeric_cast<uint32_t>(ddat->Elements.size());
				break;
			}
		case DataTypes_multidimarray_t:
			{
				RR_INTRUSIVE_PTR<MessageElementMultiDimArray> mdat = RR_STATIC_POINTER_CAST<MessageElementMultiDimArray>(dat);
				if (!mdat) throw DataTypeException("");
				DataCount = boost::numeric_cast<uint32_t>(mdat->Elements.size());
				break;
			}
		case DataTypes_list_t:
			{
				RR_INTRUSIVE_PTR<MessageElementList> ddat = RR_STATIC_POINTER_CAST<MessageElementList>(dat);
				if (!ddat) throw DataTypeException("");
				DataCount = boost::numeric_cast<uint32_t>(ddat->Elements.size());
				break;
			}
		case DataTypes_pod_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPod> sdat = RR_STATIC_POINTER_CAST<MessageElementPod>(dat);
			if (!sdat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(sdat->Elements.size());
			ElementTypeName = sdat->GetTypeString();
			break;
		}
		case DataTypes_pod_array_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPodArray> sdat = RR_STATIC_POINTER_CAST<MessageElementPodArray>(dat);
			if (!sdat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(sdat->Elements.size());
			ElementTypeName = sdat->GetTypeString();
			break;
		}
		case DataTypes_pod_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPodMultiDimArray> sdat = RR_STATIC_POINTER_CAST<MessageElementPodMultiDimArray>(dat);
			if (!sdat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(sdat->Elements.size());
			ElementTypeName = sdat->GetTypeString();
			break;
		}
		case DataTypes_namedarray_array_t:
		{
			RR_INTRUSIVE_PTR<MessageElementNamedArray> sdat = RR_STATIC_POINTER_CAST<MessageElementNamedArray>(dat);
			if (!sdat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(sdat->Elements.size());
			ElementTypeName = sdat->GetTypeString();
			break;
		}
		case DataTypes_namedarray_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementNamedMultiDimArray> sdat = RR_STATIC_POINTER_CAST<MessageElementNamedMultiDimArray>(dat);
			if (!sdat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(sdat->Elements.size());
			ElementTypeName = sdat->GetTypeString();
			break;
		}
		default:
			throw DataTypeException("Unknown data type");
		}
		
		ElementSize = ComputeSize();

	}

	void MessageElement::Write( ArrayBinaryWriter &w)
	{

		UpdateData();

		w.PushRelativeLimit(ElementSize);

		w.WriteNumber(ElementSize);
		w.WriteNumber(boost::numeric_cast<uint16_t>(ArrayBinaryWriter::GetStringByteCount8(ElementName)));
		w.WriteString8(ElementName);
		w.WriteNumber(static_cast<uint16_t>(ElementType));
		w.WriteNumber(boost::numeric_cast<uint16_t>(ArrayBinaryWriter::GetStringByteCount8(ElementTypeName)));
		w.WriteString8(ElementTypeName);
		w.WriteNumber(boost::numeric_cast<uint16_t>(ArrayBinaryWriter::GetStringByteCount8(MetaData)));
		w.WriteString8(MetaData);
		w.WriteNumber(boost::numeric_cast<uint32_t>(DataCount));


		switch (ElementType)
		{
		case DataTypes_void_t:
			DataCount=0;
			break;
		case DataTypes_double_t:
		case DataTypes_single_t:
		case DataTypes_int8_t:
		case DataTypes_uint8_t:
		case DataTypes_int16_t:
		case DataTypes_uint16_t:
		case DataTypes_int32_t:
		case DataTypes_uint32_t:
		case DataTypes_int64_t:
		case DataTypes_uint64_t:
		case DataTypes_string_t:
		case DataTypes_cdouble_t:
		case DataTypes_csingle_t:
		case DataTypes_bool_t:
			{
				RR_INTRUSIVE_PTR<RRBaseArray> rdat = RR_STATIC_POINTER_CAST<RRBaseArray>(dat);
				if (!rdat) throw DataTypeException("");				
				w.WriteArray(rdat);
				break;
			}
		case DataTypes_structure_t:
			{
				RR_INTRUSIVE_PTR<MessageElementStructure> sdat = RR_STATIC_POINTER_CAST<MessageElementStructure>(dat);
				if (!sdat) throw DataTypeException("");
				BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, sdat->Elements)
					e->Write(w);
			break;
			}
		case DataTypes_vector_t:
			{
				RR_INTRUSIVE_PTR<MessageElementMap<int32_t> > vdat = RR_STATIC_POINTER_CAST<MessageElementMap<int32_t> >(dat);
				if (!vdat) throw DataTypeException("");
				BOOST_FOREACH (RR_INTRUSIVE_PTR<MessageElement>& e, vdat->Elements)
					e->Write(w);
				break;
			}
		case DataTypes_dictionary_t:
			{
				RR_INTRUSIVE_PTR<MessageElementMap<std::string> > ddat = RR_STATIC_POINTER_CAST<MessageElementMap<std::string> >(dat);
				if (!ddat) throw DataTypeException("");
				BOOST_FOREACH (RR_INTRUSIVE_PTR<MessageElement>& e, ddat->Elements)
					e->Write(w);
				break;
			}
		case DataTypes_multidimarray_t:
			{
				RR_INTRUSIVE_PTR<MessageElementMultiDimArray> mdat = RR_STATIC_POINTER_CAST<MessageElementMultiDimArray>(dat);
				if (!mdat) throw DataTypeException("");
				BOOST_FOREACH (RR_INTRUSIVE_PTR<MessageElement>& e, mdat->Elements)
					e->Write(w);
				break;
			}
		case DataTypes_list_t:
			{
				RR_INTRUSIVE_PTR<MessageElementList> ddat = RR_STATIC_POINTER_CAST<MessageElementList>(dat);
				if (!ddat) throw DataTypeException("");
				BOOST_FOREACH (RR_INTRUSIVE_PTR<MessageElement>& e, ddat->Elements)
					e->Write(w);
				break;
			}
		case DataTypes_pod_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPod> sdat = RR_STATIC_POINTER_CAST<MessageElementPod>(dat);
			if (!sdat) throw DataTypeException("");
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, sdat->Elements)
				e->Write(w);
			break;
		}
		case DataTypes_pod_array_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPodArray> sdat = RR_STATIC_POINTER_CAST<MessageElementPodArray>(dat);
			if (!sdat) throw DataTypeException("");
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, sdat->Elements)
				e->Write(w);
			break;
		}
		case DataTypes_pod_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPodMultiDimArray> sdat = RR_STATIC_POINTER_CAST<MessageElementPodMultiDimArray>(dat);
			if (!sdat) throw DataTypeException("");
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, sdat->Elements)
				e->Write(w);
			break;
		}
		case DataTypes_namedarray_array_t:
		{
			RR_INTRUSIVE_PTR<MessageElementNamedArray> sdat = RR_STATIC_POINTER_CAST<MessageElementNamedArray>(dat);
			if (!sdat) throw DataTypeException("");
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, sdat->Elements)
				e->Write(w);
			break;
		}
		case DataTypes_namedarray_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementNamedMultiDimArray> sdat = RR_STATIC_POINTER_CAST<MessageElementNamedMultiDimArray>(dat);
			if (!sdat) throw DataTypeException("");
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, sdat->Elements)
				e->Write(w);
			break;
		}
		default:
			throw DataTypeException("Unknown data type");
		}

		if (w.DistanceFromLimit()!=0) throw DataSerializationException("Error in message format");
		w.PopLimit();
	}

	void MessageElement::Read(ArrayBinaryReader &r)
	{
		ElementSize = r.ReadNumber<uint32_t>();



		r.PushRelativeLimit(ElementSize - 4);

		uint16_t name_s = r.ReadNumber<uint16_t>();
		ElementName = r.ReadString8(name_s);
		ElementType = static_cast<DataTypes>(r.ReadNumber<uint16_t>());
		uint16_t nametype_s = r.ReadNumber<uint16_t>();
		ElementTypeName = r.ReadString8(nametype_s);
		uint16_t metadata_s = r.ReadNumber<uint16_t>();
		MetaData = r.ReadString8(metadata_s);
		DataCount = r.ReadNumber<uint32_t>();

		switch (ElementType)
		{
		case DataTypes_void_t:			
			break;
		case DataTypes_double_t:
		case DataTypes_single_t:
		case DataTypes_int8_t:
		case DataTypes_uint8_t:
		case DataTypes_int16_t:
		case DataTypes_uint16_t:
		case DataTypes_int32_t:
		case DataTypes_uint32_t:
		case DataTypes_int64_t:
		case DataTypes_uint64_t:
		case DataTypes_string_t:
		case DataTypes_cdouble_t:
		case DataTypes_csingle_t:
		case DataTypes_bool_t:
		{
			if (boost::numeric_cast<int32_t>(RRArrayElementSize(ElementType)*DataCount) > r.DistanceFromLimit())
			{
				throw DataSerializationException("Error in message format");
			}

			RR_INTRUSIVE_PTR<RRBaseArray> d = AllocateRRArrayByType(ElementType, DataCount);
			r.ReadArray(d);
			dat = d;
			break;
		}
		case DataTypes_structure_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read(r);
				l.push_back(m);
			}

			dat = CreateMessageElementStructure(ElementTypeName, l);
			break;
		}
		case DataTypes_vector_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read(r);
				l.push_back(m);
			}

			dat = CreateMessageElementMap<int32_t>(l);
			break;
		}
		case DataTypes_dictionary_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read(r);
				l.push_back(m);
			}

			dat = CreateMessageElementMap<std::string>(l);
			break;
		}
		case DataTypes_multidimarray_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read(r);
				l.push_back(m);
			}

			dat = CreateMessageElementMultiDimArray(l);
			break;
		}
		case DataTypes_list_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read(r);
				l.push_back(m);
			}

			dat = CreateMessageElementList(l);
			break;
		}
		case DataTypes_pod_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read(r);
				l.push_back(m);
			}

			dat = CreateMessageElementPod(l);
			break;
		}
		case DataTypes_pod_array_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read(r);
				l.push_back(m);
			}

			dat = CreateMessageElementPodArray(ElementTypeName,l);
			break;
		}
		case DataTypes_pod_multidimarray_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read(r);
				l.push_back(m);
			}

			dat = CreateMessageElementPodMultiDimArray(ElementTypeName,l);
			break;
		}
		case DataTypes_namedarray_array_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read(r);
				l.push_back(m);
			}

			dat = CreateMessageElementNamedArray(ElementTypeName, l);
			break;
		}
		case DataTypes_namedarray_multidimarray_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read(r);
				l.push_back(m);
			}

			dat = CreateMessageElementNamedMultiDimArray(ElementTypeName, l);
			break;
		}
		default:
			throw DataTypeException("Unknown data type");
		}				

		if (r.DistanceFromLimit() != 0) throw DataSerializationException("Error in message format");
		r.PopLimit();

	}

	uint32_t MessageElement::ComputeSize3()
	{
		size_t s = 3;

		if (ElementFlags & MessageElementFlags_ELEMENT_NAME_STR)
		{
			s += boost::numeric_cast<uint32_t>(ArrayBinaryWriter::GetStringByteCount8WithXLen(ElementName));
		}
		if (ElementFlags & MessageElementFlags_ELEMENT_NAME_CODE)
		{
			s += ArrayBinaryWriter::GetUintXByteCount(ElementNameCode);
		}
		
		if (ElementFlags & MessageElementFlags_ELEMENT_NUMBER)
		{
			s += ArrayBinaryWriter::GetIntXByteCount(ElementNumber);
		}

		if (ElementFlags & MessageElementFlags_ELEMENT_TYPE_NAME_STR)
		{
			s += boost::numeric_cast<uint32_t>(ArrayBinaryWriter::GetStringByteCount8WithXLen(ElementTypeName));
		}
		if (ElementFlags & MessageElementFlags_ELEMENT_TYPE_NAME_CODE)
		{
			s += ArrayBinaryWriter::GetUintXByteCount(ElementTypeNameCode);
		}

		if (ElementFlags & MessageElementFlags_SEQUENCE_NUMBER)
		{
			s += ArrayBinaryWriter::GetUintXByteCount(SequenceNumber);
		}
		
		if (ElementFlags & MessageElementFlags_META_INFO)
		{
			s += boost::numeric_cast<uint32_t>(ArrayBinaryWriter::GetStringByteCount8WithXLen(MetaData));
		}		

		switch (ElementType)
		{
		case DataTypes_void_t:
			break;
		case DataTypes_double_t:
		case DataTypes_single_t:
		case DataTypes_int8_t:
		case DataTypes_uint8_t:
		case DataTypes_int16_t:
		case DataTypes_uint16_t:
		case DataTypes_int32_t:
		case DataTypes_uint32_t:
		case DataTypes_int64_t:
		case DataTypes_uint64_t:
		case DataTypes_string_t:
		case DataTypes_cdouble_t:
		case DataTypes_csingle_t:
		case DataTypes_bool_t:
			s += DataCount * RRArrayElementSize(ElementType);
			break;
		case DataTypes_structure_t:
		{
			RR_INTRUSIVE_PTR<MessageElementStructure> d = rr_cast<MessageElementStructure>(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData3();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_vector_t:
		{
			RR_INTRUSIVE_PTR<MessageElementMap<int32_t> > d = rr_cast<MessageElementMap<int32_t> >(GetData());

			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData3();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_dictionary_t:
		{
			RR_INTRUSIVE_PTR<MessageElementMap<std::string> > d = rr_cast<MessageElementMap<std::string> >(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData3();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementMultiDimArray> d = rr_cast<MessageElementMultiDimArray>(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData3();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_list_t:
		{
			RR_INTRUSIVE_PTR<MessageElementList > d = rr_cast<MessageElementList >(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData3();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_pod_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPod > d = rr_cast<MessageElementPod >(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData3();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_pod_array_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPodArray > d = rr_cast<MessageElementPodArray >(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData3();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_pod_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPodMultiDimArray > d = rr_cast<MessageElementPodMultiDimArray >(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData3();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_namedarray_array_t:
		{
			RR_INTRUSIVE_PTR<MessageElementNamedArray > d = rr_cast<MessageElementNamedArray>(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData3();
				s += e->ElementSize;
			}
			break;
		}
		case DataTypes_namedarray_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementNamedMultiDimArray > d = rr_cast<MessageElementNamedMultiDimArray>(GetData());
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, d->Elements)
			{
				e->UpdateData3();
				s += e->ElementSize;
			}
			break;
		}
		default:
			throw DataTypeException("Unknown data type");

		}
		
		

		s += ArrayBinaryWriter::GetUintXByteCount(DataCount);

		s = ArrayBinaryWriter::GetSizePlusUintX(s);

		return boost::numeric_cast<uint32_t>(s);
	}

	void MessageElement::UpdateData3()
	{	

		std::string datatype;
		if (!dat)
			ElementType = DataTypes_void_t;
		else
			ElementType = dat->GetTypeID();

		ElementTypeName = "";
		switch (ElementType)
		{
		case DataTypes_void_t:
			DataCount = 0;
			break;
		case DataTypes_double_t:
		case DataTypes_single_t:
		case DataTypes_int8_t:
		case DataTypes_uint8_t:
		case DataTypes_int16_t:
		case DataTypes_uint16_t:
		case DataTypes_int32_t:
		case DataTypes_uint32_t:
		case DataTypes_int64_t:
		case DataTypes_uint64_t:
		case DataTypes_string_t:
		case DataTypes_cdouble_t:
		case DataTypes_csingle_t:
		case DataTypes_bool_t:
		{
			RR_INTRUSIVE_PTR<RRBaseArray> rdat = RR_STATIC_POINTER_CAST<RRBaseArray>(dat);
			if (!rdat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(rdat->size());
			break;
		}
		case DataTypes_structure_t:
		{
			RR_INTRUSIVE_PTR<MessageElementStructure> sdat = RR_STATIC_POINTER_CAST<MessageElementStructure>(dat);
			if (!sdat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(sdat->Elements.size());
			//If flags have ElementTypeNameCode set, assume that the ElementTypeName has already been encoded
			if ((ElementFlags & MessageElementFlags_ELEMENT_TYPE_NAME_CODE) == 0)
			{
				ElementTypeName = sdat->GetTypeString();
			}
			break;
		}
		case DataTypes_vector_t:
		{
			RR_INTRUSIVE_PTR<MessageElementMap<int32_t> > vdat = RR_STATIC_POINTER_CAST<MessageElementMap<int32_t> >(dat);
			if (!vdat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(vdat->Elements.size());
			break;
		}
		case DataTypes_dictionary_t:
		{
			RR_INTRUSIVE_PTR<MessageElementMap<std::string> > ddat = RR_STATIC_POINTER_CAST<MessageElementMap<std::string> >(dat);
			if (!ddat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(ddat->Elements.size());
			break;
		}
		case DataTypes_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementMultiDimArray> mdat = RR_STATIC_POINTER_CAST<MessageElementMultiDimArray>(dat);
			if (!mdat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(mdat->Elements.size());
			break;
		}
		case DataTypes_list_t:
		{
			RR_INTRUSIVE_PTR<MessageElementList> ddat = RR_STATIC_POINTER_CAST<MessageElementList>(dat);
			if (!ddat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(ddat->Elements.size());
			break;
		}
		case DataTypes_pod_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPod> sdat = RR_STATIC_POINTER_CAST<MessageElementPod>(dat);
			if (!sdat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(sdat->Elements.size());
			ElementTypeName = sdat->GetTypeString();
			break;
		}
		case DataTypes_pod_array_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPodArray> sdat = RR_STATIC_POINTER_CAST<MessageElementPodArray>(dat);
			if (!sdat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(sdat->Elements.size());
			ElementTypeName = sdat->GetTypeString();
			break;
		}
		case DataTypes_pod_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPodMultiDimArray> sdat = RR_STATIC_POINTER_CAST<MessageElementPodMultiDimArray>(dat);
			if (!sdat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(sdat->Elements.size());
			ElementTypeName = sdat->GetTypeString();
			break;
		}
		case DataTypes_namedarray_array_t:
		{
			RR_INTRUSIVE_PTR<MessageElementNamedArray> sdat = RR_STATIC_POINTER_CAST<MessageElementNamedArray>(dat);
			if (!sdat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(sdat->Elements.size());
			ElementTypeName = sdat->GetTypeString();
			break;
		}
		case DataTypes_namedarray_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementNamedMultiDimArray> sdat = RR_STATIC_POINTER_CAST<MessageElementNamedMultiDimArray>(dat);
			if (!sdat) throw DataTypeException("");
			DataCount = boost::numeric_cast<uint32_t>(sdat->Elements.size());
			ElementTypeName = sdat->GetTypeString();
			break;
		}
		default:
			throw DataTypeException("Unknown data type");
		}

		//TODO: string table here

		/*if (ElementName.size() > 0)
		{
			ElementFlags |= MessageElementFlags_ELEMENT_NAME_STR;
		}
		else
		{
			ElementFlags &= ~MessageElementFlags_ELEMENT_NAME_STR;
		}
		if (ElementNameCode != 0)
		{
			throw NotImplementedException("Not implemented");
		}
		ElementFlags &= ~MessageElementFlags_ELEMENT_NAME_CODE;
		*/

		if ((ElementFlags & ( MessageElementFlags_ELEMENT_NAME_STR | MessageElementFlags_ELEMENT_NAME_CODE))
			&& (ElementFlags & MessageElementFlags_ELEMENT_NUMBER))
		{
			throw ProtocolException("Cannot set both element name and number");
		}

		if (ElementTypeName.size() > 0)
		{
			ElementFlags |= MessageElementFlags_ELEMENT_TYPE_NAME_STR;
		}
		else
		{
			ElementFlags &= ~MessageElementFlags_ELEMENT_TYPE_NAME_STR;
		}		

		if (MetaData.size() > 0)
		{
			ElementFlags |= MessageElementFlags_META_INFO;
		}
		else
		{
			ElementFlags &= ~MessageElementFlags_META_INFO;
		}

		ElementSize = ComputeSize3();

	}

	void MessageElement::Write3(ArrayBinaryWriter &w, const uint16_t& version_minor)
	{
		if (version_minor != 0) throw ProtocolException("Invalid Message 3 version minor");

		UpdateData3();

		w.PushRelativeLimit(ElementSize);

		w.WriteUintX(ElementSize);
		w.WriteNumber(ElementFlags);
		if (ElementFlags & MessageElementFlags_ELEMENT_NAME_STR)
			w.WriteString8WithXLen(ElementName);
		if (ElementFlags & MessageElementFlags_ELEMENT_NAME_CODE)
			w.WriteUintX(ElementNameCode);
		if (ElementFlags & MessageElementFlags_ELEMENT_NUMBER)
			w.WriteIntX(ElementNumber);
		w.WriteNumber(static_cast<uint16_t>(ElementType));	
		if (ElementFlags & MessageElementFlags_ELEMENT_TYPE_NAME_STR)
			w.WriteString8WithXLen(ElementTypeName);
		if (ElementFlags & MessageElementFlags_ELEMENT_TYPE_NAME_CODE)
			w.WriteUintX(ElementTypeNameCode);
		if (ElementFlags & MessageElementFlags_SEQUENCE_NUMBER)
			w.WriteUintX(SequenceNumber);
		if (ElementFlags & MessageElementFlags_META_INFO)
			w.WriteString8WithXLen(MetaData);
		w.WriteUintX(boost::numeric_cast<uint32_t>(DataCount));

		switch (ElementType)
		{
		case DataTypes_void_t:			
			break;
		case DataTypes_double_t:
		case DataTypes_single_t:
		case DataTypes_int8_t:
		case DataTypes_uint8_t:
		case DataTypes_int16_t:
		case DataTypes_uint16_t:
		case DataTypes_int32_t:
		case DataTypes_uint32_t:
		case DataTypes_int64_t:
		case DataTypes_uint64_t:
		case DataTypes_string_t:
		case DataTypes_cdouble_t:
		case DataTypes_csingle_t:
		case DataTypes_bool_t:
		{
			RR_INTRUSIVE_PTR<RRBaseArray> rdat = RR_STATIC_POINTER_CAST<RRBaseArray>(dat);
			if (!rdat) throw DataTypeException("");			
			w.WriteArray(rdat);
			break;
		}
		case DataTypes_structure_t:
		{
			RR_INTRUSIVE_PTR<MessageElementStructure> sdat = RR_STATIC_POINTER_CAST<MessageElementStructure>(dat);
			if (!sdat) throw DataTypeException("");
			BOOST_FOREACH (RR_INTRUSIVE_PTR<MessageElement>& e, sdat->Elements)
				e->Write3(w, version_minor);
			break;
		}
		case DataTypes_vector_t:
		{
			RR_INTRUSIVE_PTR<MessageElementMap<int32_t> > vdat = RR_STATIC_POINTER_CAST<MessageElementMap<int32_t> >(dat);
			if (!vdat) throw DataTypeException("");
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, vdat->Elements)
				e->Write3(w, version_minor);
			break;
		}
		case DataTypes_dictionary_t:
		{
			RR_INTRUSIVE_PTR<MessageElementMap<std::string> > ddat = RR_STATIC_POINTER_CAST<MessageElementMap<std::string> >(dat);
			if (!ddat) throw DataTypeException("");
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, ddat->Elements)
				e->Write3(w, version_minor);
			break;
		}
		case DataTypes_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementMultiDimArray> mdat = RR_STATIC_POINTER_CAST<MessageElementMultiDimArray>(dat);
			if (!mdat) throw DataTypeException("");
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, mdat->Elements)
				e->Write3(w, version_minor);
			break;
		}
		case DataTypes_list_t:
		{
			RR_INTRUSIVE_PTR<MessageElementList> ddat = RR_STATIC_POINTER_CAST<MessageElementList>(dat);
			if (!ddat) throw DataTypeException("");
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, ddat->Elements)
				e->Write3(w, version_minor);
			break;
		}
		case DataTypes_pod_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPod> sdat = RR_STATIC_POINTER_CAST<MessageElementPod>(dat);
			if (!sdat) throw DataTypeException("");
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, sdat->Elements)
				e->Write3(w, version_minor);
			break;
		}
		case DataTypes_pod_array_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPodArray> sdat = RR_STATIC_POINTER_CAST<MessageElementPodArray>(dat);
			if (!sdat) throw DataTypeException("");
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, sdat->Elements)
				e->Write3(w, version_minor);
			break;
		}
		case DataTypes_pod_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPodMultiDimArray> sdat = RR_STATIC_POINTER_CAST<MessageElementPodMultiDimArray>(dat);
			if (!sdat) throw DataTypeException("");
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, sdat->Elements)
				e->Write3(w, version_minor);
			break;
		}
		case DataTypes_namedarray_array_t:
		{
			RR_INTRUSIVE_PTR<MessageElementNamedArray> sdat = RR_STATIC_POINTER_CAST<MessageElementNamedArray>(dat);
			if (!sdat) throw DataTypeException("");
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, sdat->Elements)
				e->Write3(w, version_minor);
			break;
		}
		case DataTypes_namedarray_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementNamedMultiDimArray> sdat = RR_STATIC_POINTER_CAST<MessageElementNamedMultiDimArray>(dat);
			if (!sdat) throw DataTypeException("");
			BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, sdat->Elements)
				e->Write3(w, version_minor);
			break;
		}
		default:
			throw DataTypeException("Unknown data type");
		}

		if (w.DistanceFromLimit() != 0) throw DataSerializationException("Error in message format");
		w.PopLimit();		

	}

	void MessageElement::Read3(ArrayBinaryReader &r, const uint16_t& version_minor)
	{
		if (version_minor != 0) throw ProtocolException("Invalid Message 3 version minor");

		ElementSize = r.ReadUintX();

		r.PushRelativeLimit(ElementSize - ArrayBinaryWriter::GetUintXByteCount(ElementSize));

		ElementFlags = r.ReadNumber<uint8_t>();
		
		if (ElementFlags & MessageElementFlags_ELEMENT_NAME_STR)
		{
			uint32_t name_s = r.ReadUintX();
			ElementName = r.ReadString8(name_s);
		}

		if (ElementFlags & MessageElementFlags_ELEMENT_NAME_CODE)
		{
			ElementNameCode = r.ReadUintX();
		}

		if (ElementFlags & MessageElementFlags_ELEMENT_NUMBER)
		{
			ElementNumber = r.ReadIntX();
		}

		ElementType = static_cast<DataTypes>(r.ReadNumber<uint16_t>());

		if (ElementFlags & MessageElementFlags_ELEMENT_TYPE_NAME_STR)
		{			
			uint32_t nametype_s = r.ReadUintX();
			ElementTypeName = r.ReadString8(nametype_s);
		}
		
		if (ElementFlags & MessageElementFlags_ELEMENT_TYPE_NAME_CODE)
		{
			ElementTypeNameCode = r.ReadUintX();
		}

		if (ElementFlags & MessageElementFlags_SEQUENCE_NUMBER)
		{
			SequenceNumber = r.ReadUintX();
		}

		if (ElementFlags & MessageElementFlags_META_INFO)
		{
			uint32_t metadata_s = r.ReadUintX();
			MetaData = r.ReadString8(metadata_s);
		}
		
		DataCount = r.ReadUintX();

		switch (ElementType)
		{
		case DataTypes_void_t:
			break;
		case DataTypes_double_t:
		case DataTypes_single_t:
		case DataTypes_int8_t:
		case DataTypes_uint8_t:
		case DataTypes_int16_t:
		case DataTypes_uint16_t:
		case DataTypes_int32_t:
		case DataTypes_uint32_t:
		case DataTypes_int64_t:
		case DataTypes_uint64_t:
		case DataTypes_string_t:
		case DataTypes_cdouble_t:
		case DataTypes_csingle_t:
		case DataTypes_bool_t:
		{
			if (boost::numeric_cast<int32_t>(RRArrayElementSize(ElementType)*DataCount) > r.DistanceFromLimit())
			{
				throw DataSerializationException("Error in message format");
			}

			RR_INTRUSIVE_PTR<RRBaseArray> d = AllocateRRArrayByType(ElementType, DataCount);
			r.ReadArray(d);
			dat = d;
			break;
		}
		case DataTypes_structure_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read3(r, version_minor);
				l.push_back(m);

			}

			dat = CreateMessageElementStructure(ElementTypeName, l);
			break;
		}
		case DataTypes_vector_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read3(r, version_minor);
				l.push_back(m);

			}

			dat = CreateMessageElementMap<int32_t>(l);
			break;
		}
		case DataTypes_dictionary_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read3(r, version_minor);
				l.push_back(m);

			}

			dat = CreateMessageElementMap<std::string>(l);
			break;
		}
		case DataTypes_multidimarray_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read3(r, version_minor);
				l.push_back(m);

			}

			dat = CreateMessageElementMultiDimArray(l);
			break;
		}
		case DataTypes_list_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read3(r, version_minor);
				l.push_back(m);

			}

			dat = CreateMessageElementList(l);
			break;
		}
		case DataTypes_pod_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read3(r, version_minor);
				l.push_back(m);

			}
			dat = CreateMessageElementPod(l);
			break;
		}
		case DataTypes_pod_array_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read3(r, version_minor);
				l.push_back(m);

			}
			dat = CreateMessageElementPodArray(ElementTypeName, l);
			break;
		}
		case DataTypes_pod_multidimarray_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read3(r, version_minor);
				l.push_back(m);

			}
			dat = CreateMessageElementPodMultiDimArray(ElementTypeName, l);
			break;
		}
		case DataTypes_namedarray_array_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read3(r, version_minor);
				l.push_back(m);

			}
			dat = CreateMessageElementNamedArray(ElementTypeName, l);
			break;
		}
		case DataTypes_namedarray_multidimarray_t:
		{
			std::vector<RR_INTRUSIVE_PTR<MessageElement> > l;
			for (size_t i = 0; i < DataCount; i++)
			{
				RR_INTRUSIVE_PTR<MessageElement> m = CreateMessageElement();
				m->Read3(r, version_minor);
				l.push_back(m);

			}
			dat = CreateMessageElementNamedMultiDimArray(ElementTypeName, l);
			break;
		}
		default:
			throw ProtocolException("Invalid message element type");
		}
		
		if (r.DistanceFromLimit() != 0) throw DataSerializationException("Error in message format");
		r.PopLimit();
	}	

	RR_INTRUSIVE_PTR<MessageElement> MessageElement::FindElement(std::vector<RR_INTRUSIVE_PTR<MessageElement> > &m, const std::string& name)
	{
		std::vector<RR_INTRUSIVE_PTR<MessageElement> >::iterator m1 = boost::find_if(m,
			boost::bind(&MessageElement::ElementName, _1) == name);

		if (m1 == m.end()) throw MessageElementNotFoundException("Element " + name + " not found.");

		return *m1;
	}

	bool MessageElement::TryFindElement(std::vector<RR_INTRUSIVE_PTR<MessageElement> > &m, const std::string& name, RR_INTRUSIVE_PTR<MessageElement>& elem)
	{
		std::vector<RR_INTRUSIVE_PTR<MessageElement> >::iterator m1 = boost::find_if(m,
			boost::bind(&MessageElement::ElementName, _1) == name);

		if (m1 == m.end()) return false;

		elem= *m1;
		return true;

	}

	bool MessageElement::ContainsElement(std::vector<RR_INTRUSIVE_PTR<MessageElement> > &m, const std::string& name)
	{
		std::vector<RR_INTRUSIVE_PTR<MessageElement> >::iterator m1 = boost::find_if(m,
			boost::bind(&MessageElement::ElementName, _1) == name);

		if (m1 == m.end()) return false;

		return true;
	}

	std::string MessageElement::CastDataToString()
	{
		RR_INTRUSIVE_PTR<RRArray<char> > datarr=CastData<RRArray<char> >();
		return RRArrayToString(datarr);
	}
	

	MessageElementStructure::MessageElementStructure(const std::string& type_, const std::vector<RR_INTRUSIVE_PTR<MessageElement> > &elements_)
	{
		Elements = elements_;
		Type = type_;
	}

	MessageElementMultiDimArray::MessageElementMultiDimArray(const std::vector<RR_INTRUSIVE_PTR<MessageElement> > &e)
	{
	   Elements = e;
	}

	MessageElementPod::MessageElementPod(const std::vector<RR_INTRUSIVE_PTR<MessageElement> > &elements_)
	{
		Elements = elements_;
	}

	MessageElementPodArray::MessageElementPodArray(const std::string& type_, const std::vector<RR_INTRUSIVE_PTR<MessageElement> > &elements_)
	{
		Elements = elements_;
		Type = type_;
	}

	MessageElementNamedMultiDimArray::MessageElementNamedMultiDimArray(const std::string& type_, const std::vector<RR_INTRUSIVE_PTR<MessageElement> > &elements_)
	{
		Elements = elements_;
		Type = type_;
	}

	MessageElementNamedArray::MessageElementNamedArray(const std::string& type_, const std::vector<RR_INTRUSIVE_PTR<MessageElement> > &elements_)
	{
		Elements = elements_;
		Type = type_;
	}

	MessageElementPodMultiDimArray::MessageElementPodMultiDimArray(const std::string& type_, const std::vector<RR_INTRUSIVE_PTR<MessageElement> > &elements_)
	{
		Elements = elements_;
		Type = type_;
	}


	ROBOTRACONTEUR_CORE_API RR_INTRUSIVE_PTR<Message> CreateMessage()
	{
		return new Message();
	}
	ROBOTRACONTEUR_CORE_API RR_INTRUSIVE_PTR<MessageHeader> CreateMessageHeader()
	{
		return new MessageHeader();
	}
	ROBOTRACONTEUR_CORE_API RR_INTRUSIVE_PTR<MessageEntry> CreateMessageEntry()
	{
		return new MessageEntry();
	}
	ROBOTRACONTEUR_CORE_API RR_INTRUSIVE_PTR<MessageEntry> CreateMessageEntry(MessageEntryType t, const std::string& n)
	{
		return new MessageEntry(t, n);
	}
	ROBOTRACONTEUR_CORE_API RR_INTRUSIVE_PTR<MessageElement> CreateMessageElement()
	{
		return new MessageElement();
	}
	ROBOTRACONTEUR_CORE_API RR_INTRUSIVE_PTR<MessageElement> CreateMessageElement(const std::string& name, RR_INTRUSIVE_PTR<MessageElementData> datin)
	{
		return new MessageElement(name, datin);
	}
	ROBOTRACONTEUR_CORE_API RR_INTRUSIVE_PTR<MessageElementStructure> CreateMessageElementStructure(const std::string& type_, const std::vector<RR_INTRUSIVE_PTR<MessageElement> > &elements_)
	{
		return new MessageElementStructure(type_, elements_);
	}
	ROBOTRACONTEUR_CORE_API RR_INTRUSIVE_PTR<MessageElementMultiDimArray> CreateMessageElementMultiDimArray(const std::vector<RR_INTRUSIVE_PTR<MessageElement> > &e)
	{
		return new MessageElementMultiDimArray(e);
	}
	ROBOTRACONTEUR_CORE_API RR_INTRUSIVE_PTR<MessageElementList> CreateMessageElementList(const std::vector<RR_INTRUSIVE_PTR<MessageElement> >& e)
	{
		return new MessageElementList(e);
	}
	ROBOTRACONTEUR_CORE_API RR_INTRUSIVE_PTR<MessageElementPod> CreateMessageElementPod(const std::vector<RR_INTRUSIVE_PTR<MessageElement> > &elements_)
	{
		return new MessageElementPod(elements_);
	}
	ROBOTRACONTEUR_CORE_API RR_INTRUSIVE_PTR<MessageElementPodArray> CreateMessageElementPodArray(const std::string& type_, const std::vector<RR_INTRUSIVE_PTR<MessageElement> > &elements_)
	{
		return new MessageElementPodArray(type_, elements_);
	}
	ROBOTRACONTEUR_CORE_API RR_INTRUSIVE_PTR<MessageElementPodMultiDimArray> CreateMessageElementPodMultiDimArray(const std::string& type_, const std::vector<RR_INTRUSIVE_PTR<MessageElement> > &elements_)
	{
		return new MessageElementPodMultiDimArray(type_, elements_);
	}
	ROBOTRACONTEUR_CORE_API RR_INTRUSIVE_PTR<MessageElementNamedArray> CreateMessageElementNamedArray(const std::string& type_, const std::vector<RR_INTRUSIVE_PTR<MessageElement> > &elements_)
	{
		return new MessageElementNamedArray(type_, elements_);
	}
	ROBOTRACONTEUR_CORE_API RR_INTRUSIVE_PTR<MessageElementNamedMultiDimArray> CreateMessageElementNamedMultiDimArray(const std::string& type_, const std::vector<RR_INTRUSIVE_PTR<MessageElement> > &elements_)
	{
		return new MessageElementNamedMultiDimArray(type_, elements_);
	}



	RR_INTRUSIVE_PTR<Message> ShallowCopyMessage(RR_INTRUSIVE_PTR<Message> m)
	{
		if (!m) return RR_INTRUSIVE_PTR<Message>();

		RR_INTRUSIVE_PTR<Message> m2 = CreateMessage();
		if (m->header)
		{
			RR_INTRUSIVE_PTR<MessageHeader>& h = m->header;
			RR_INTRUSIVE_PTR<MessageHeader> h2 = CreateMessageHeader();
			h2->MessageSize = h->MessageSize;
			h2->HeaderSize = h->HeaderSize;
			h2->MessageFlags = h->MessageFlags;
			h2->SubstreamID = h->SubstreamID;
			h2->SubstreamSequenceNumber = h->SubstreamSequenceNumber;
			h2->FragmentHeader = h->FragmentHeader;
			h2->SenderEndpoint = h->SenderEndpoint;
			h2->ReceiverEndpoint = h->ReceiverEndpoint;
			h2->SenderNodeName = h->SenderNodeName;
			h2->ReceiverNodeName = h->ReceiverNodeName;
			h2->SenderNodeID = h->SenderNodeID;
			h2->ReceiverNodeID = h->ReceiverNodeID;
			h2->MetaData = h->MetaData;
			h2->EntryCount = h->EntryCount;
			h2->MessageID = h->MessageID;
			h2->MessageResID = h->MessageResID;
			h2->StringTable = h->StringTable;
			h2->UnreliableExpiration = h->UnreliableExpiration;
			h2->TransportSpecific = h->TransportSpecific;
			m2->header = h2;

		}

		BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageEntry>& e, m->entries)
		{
			m2->entries.push_back(ShallowCopyMessageEntry(e));
		}

		return m2;
	}

	RR_INTRUSIVE_PTR<MessageEntry> ShallowCopyMessageEntry(RR_INTRUSIVE_PTR<MessageEntry> mm)
	{
		if (!mm) return RR_INTRUSIVE_PTR<MessageEntry>();

		RR_INTRUSIVE_PTR<MessageEntry> mm2 = CreateMessageEntry();
		mm2->EntrySize = mm->EntrySize;
		mm2->EntryFlags = mm->EntryFlags;
		mm2->EntryType = mm->EntryType;
		mm2->ServicePath = mm->ServicePath;
		mm2->ServicePathCode = mm->ServicePathCode;
		mm2->MemberName = mm->MemberName;
		mm2->MemberNameCode = mm->MemberNameCode;
		mm2->EntryStreamID = mm->EntryStreamID;
		mm2->RequestID = mm->RequestID;
		mm2->Error = mm->Error;
		mm2->MetaData = mm->MetaData;
		mm2->EntryTimeSpec = mm->EntryTimeSpec;
		
		BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& e, mm->elements)
		{
			mm2->elements.push_back(ShallowCopyMessageElement(e));
		}

		return mm2;
	}

	RR_INTRUSIVE_PTR<MessageElement> ShallowCopyMessageElement(RR_INTRUSIVE_PTR<MessageElement> mm)
	{
		if (!mm) return RR_INTRUSIVE_PTR<MessageElement>();

		RR_INTRUSIVE_PTR<MessageElement> mm2 = CreateMessageElement();
		mm2->ElementSize = mm->ElementSize;
		mm2->ElementFlags = mm->ElementFlags;
		mm2->ElementName = mm->ElementName;
		mm2->ElementNameCode = mm->ElementNameCode;
		mm2->ElementNumber = mm->ElementNumber;
		mm2->ElementType = mm->ElementType;
		mm2->ElementTypeName = mm->ElementTypeName;
		mm2->ElementTypeNameCode = mm->ElementTypeNameCode;
		mm2->SequenceNumber = mm->SequenceNumber;
		mm2->MetaData = mm->MetaData;
		mm2->DataCount = mm->DataCount;

		switch (mm->ElementType)
		{

		case DataTypes_structure_t:
		{
			RR_INTRUSIVE_PTR<MessageElementStructure> sdat = mm->CastData<MessageElementStructure>();
			if (sdat)
			{
				std::vector<RR_INTRUSIVE_PTR<MessageElement> > v;
				BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& ee, sdat->Elements)
					v.push_back(ShallowCopyMessageElement(ee));

				RR_INTRUSIVE_PTR<MessageElementStructure> sdat2 = CreateMessageElementStructure(sdat->Type, v);
				mm2->SetData(sdat2);
			}
			break;
		}
		case DataTypes_vector_t:
		{
			RR_INTRUSIVE_PTR<MessageElementMap<int32_t> > vdat = mm->CastData<MessageElementMap<int32_t> >();
			if (vdat)
			{
				std::vector<RR_INTRUSIVE_PTR<MessageElement> > v;
				BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& ee, vdat->Elements)
					v.push_back(ShallowCopyMessageElement(ee));

				RR_INTRUSIVE_PTR<MessageElementMap<int32_t> > vdat2 = CreateMessageElementMap<int32_t>(v);
				mm2->SetData(vdat2);
			}
			break;
		}
		case DataTypes_dictionary_t:
		{
			RR_INTRUSIVE_PTR<MessageElementMap<std::string> > ddat = mm->CastData<MessageElementMap<std::string> >();
			if (ddat)
			{
				std::vector<RR_INTRUSIVE_PTR<MessageElement> > v;
				BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& ee, ddat->Elements)
					v.push_back(ShallowCopyMessageElement(ee));

				RR_INTRUSIVE_PTR<MessageElementMap<std::string> > ddat2 = CreateMessageElementMap<std::string>(v);
				mm2->SetData(ddat2);
			}
			break;
		}
		case DataTypes_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementMultiDimArray> mdat = mm->CastData<MessageElementMultiDimArray>();
			if (mdat)
			{
				std::vector<RR_INTRUSIVE_PTR<MessageElement> > v;
				BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& ee, mdat->Elements)
					v.push_back(ShallowCopyMessageElement(ee));

				RR_INTRUSIVE_PTR<MessageElementMultiDimArray> mdat2 = CreateMessageElementMultiDimArray(v);
				mm2->SetData(mdat2);
			}
			break;
		}
		case DataTypes_list_t:
		{
			RR_INTRUSIVE_PTR<MessageElementList> ddat = mm->CastData<MessageElementList>();
			if (ddat)
			{
				std::vector<RR_INTRUSIVE_PTR<MessageElement> > v;
				BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& ee, ddat->Elements)
					v.push_back(ShallowCopyMessageElement(ee));

				RR_INTRUSIVE_PTR<MessageElementList> mdat2 = CreateMessageElementList(v);
				mm2->SetData(mdat2);
			}
			break;
		}
		case DataTypes_pod_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPod> ddat = mm->CastData<MessageElementPod>();
			if (ddat)
			{
				std::vector<RR_INTRUSIVE_PTR<MessageElement> > v;
				BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& ee, ddat->Elements)
					v.push_back(ShallowCopyMessageElement(ee));

				RR_INTRUSIVE_PTR<MessageElementPod> mdat2 = CreateMessageElementPod(v);
				mm2->SetData(mdat2);
			}
			break;
		}
		case DataTypes_pod_array_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPodArray> ddat = mm->CastData<MessageElementPodArray>();
			if (ddat)
			{
				std::vector<RR_INTRUSIVE_PTR<MessageElement> > v;
				BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& ee, ddat->Elements)
					v.push_back(ShallowCopyMessageElement(ee));

				RR_INTRUSIVE_PTR<MessageElementPodArray> mdat2 = CreateMessageElementPodArray(ddat->Type, v);
				mm2->SetData(mdat2);
			}
			break;
		}
		case DataTypes_pod_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementPodMultiDimArray> ddat = mm->CastData<MessageElementPodMultiDimArray>();
			if (ddat)
			{
				std::vector<RR_INTRUSIVE_PTR<MessageElement> > v;
				BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& ee, ddat->Elements)
					v.push_back(ShallowCopyMessageElement(ee));

				RR_INTRUSIVE_PTR<MessageElementPodMultiDimArray> mdat2 = CreateMessageElementPodMultiDimArray(ddat->Type, v);
				mm2->SetData(mdat2);
			}
			break;
		}
		case DataTypes_namedarray_array_t:
		{
			RR_INTRUSIVE_PTR<MessageElementNamedArray> ddat = mm->CastData<MessageElementNamedArray>();
			if (ddat)
			{
				std::vector<RR_INTRUSIVE_PTR<MessageElement> > v;
				BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& ee, ddat->Elements)
					v.push_back(ShallowCopyMessageElement(ee));

				RR_INTRUSIVE_PTR<MessageElementNamedArray> mdat2 = CreateMessageElementNamedArray(ddat->Type, v);
				mm2->SetData(mdat2);
			}
			break;
		}
		case DataTypes_namedarray_multidimarray_t:
		{
			RR_INTRUSIVE_PTR<MessageElementNamedMultiDimArray> ddat = mm->CastData<MessageElementNamedMultiDimArray>();
			if (ddat)
			{
				std::vector<RR_INTRUSIVE_PTR<MessageElement> > v;
				BOOST_FOREACH(RR_INTRUSIVE_PTR<MessageElement>& ee, ddat->Elements)
					v.push_back(ShallowCopyMessageElement(ee));

				RR_INTRUSIVE_PTR<MessageElementNamedMultiDimArray> mdat2 = CreateMessageElementNamedMultiDimArray(ddat->Type, v);
				mm2->SetData(mdat2);
			}
			break;
		}
		default:
			mm2->SetData(mm->GetData());
			break;
		}

		return mm2;
	}
}
