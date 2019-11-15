// Copyright 2011-2019 Wason Technology, LLC
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

#pragma once

#include "RobotRaconteur/RobotRaconteurConstants.h"
#include "RobotRaconteur/DataTypes.h"
#include <boost/array.hpp>
#include <boost/uuid/uuid.hpp>

namespace RobotRaconteur
{
	
	class ROBOTRACONTEUR_CORE_API NodeID : virtual public RRObject
	{
	private:
		boost::uuids::uuid id;

	public:

		NodeID();

		NodeID(boost::array<uint8_t,16> id);

		NodeID(boost::uuids::uuid id);

		NodeID(const NodeID& id);

		NodeID(const std::string &id);

		const boost::array<uint8_t,16> ToByteArray() const;

		const boost::uuids::uuid ToUuid() const;

		virtual std::string ToString() const;

		virtual std::string ToString(const std::string& format) const;

		
		static NodeID NewUniqueID();

		bool operator == (const NodeID &id2) const;

		bool operator != (const NodeID &id2) const;

		bool operator <(const NodeID& id2) const;

		NodeID& operator =(const NodeID& id);

		bool IsAnyNode() const;
		
		static NodeID GetAny(); 

		virtual int32_t GetHashCode() const;

		virtual std::string RRType() { return "RobotRaconteur::NodeID";}

	
	};



}