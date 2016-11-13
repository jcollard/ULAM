/**                                        -*- mode:C++ -*-
 * StringPoolUser.h -  Basic String Pool management for ULAM
 *
 * Copyright (C) 2016 The Regents of the University of New Mexico.
 * Copyright (C) 2016 Ackleyshack LLC.
 *
 * This file is part of the ULAM programming language compilation system.
 *
 * The ULAM programming language compilation system is free software:
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * The ULAM programming language compilation system is distributed in
 * the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the ULAM programming language compilation system
 * software.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @license GPL-3.0+ <http://spdx.org/licenses/GPL-3.0+>
 */

/**
  \file StringPoolUser.h -  Basic String Pool management for ULAM
  \author Elenas S. Ackley.
  \author David H. Ackley.
  \date (C) 2016 All rights reserved.
  \gpl
*/


#ifndef STRINGPOOLUSER_H
#define STRINGPOOLUSER_H

#include "StringPool.h"

namespace MFM
{

  class StringPoolUser : public StringPool
  {
  public:

    StringPoolUser();

    ~StringPoolUser();

    virtual u32 getIndexForDataString(std::string str);    //< makes a new entry in maps if nonexistent

    virtual const std::string & getDataAsString(u32 dataindex);

  private:

    u32 m_runningIndex;

  };
}

#endif  /* STRINGPOOLUSER_H */
