////////////////////////////////////////////////////////////////////////////////////////
//
// Nestopia - NES/Famicom emulator written in C++
//
// Copyright (C) 2003-2008 Martin Freij
//
// This file is part of Nestopia.
//
// Nestopia is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Nestopia is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Nestopia; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////////////

#ifndef NST_IO_SCREEN_H
#define NST_IO_SCREEN_H

#pragma once

#include "NstObjectDelegate.hpp"
#include "NstString.hpp"

namespace Nestopia
{
	namespace Io
	{
		class Screen : protected HeapString
		{
			typedef Object::Delegate<void,const GenericString&,uint> Callback;

			static Callback callback;

			const uint time;

		public:

			explicit Screen(uint=0);
			~Screen();

			template<typename T,typename U>
			static void SetCallback(T data,U code)
			{
				callback = Callback(data,code);
			}

			static void UnsetCallback()
			{
				callback.Unset();
			}

			Screen& operator << (const char* t)
			{
				HeapString::operator << (t);
				return *this;
			}

			template<typename T>
			Screen& operator << (const T& t)
			{
				// HeapString::operator << (t);
				return *this;
			}

			Screen& operator << (int t)
			{
				HeapString::operator << (t);
				return *this;
			}
		};
	}
}

#endif
