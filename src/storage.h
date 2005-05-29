/*
 *  The Mana World Server
 *  Copyright 2004 The Mana World Development Team
 *
 *  This file is part of The Mana World.
 *
 *  The Mana World is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  The Mana World is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with The Mana World; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  $Id$
 */
#ifndef STORAGE_H
#define STORAGE_H

#ifdef SQLITE_SUPPORT
#include "sqlite/SQLiteWrapper.h"
#endif

#include "object.h"
#include "account.h"

/*
 * Storage
 * Storage is the resource manager
 */ 
class Storage {
        //make storage singleton
        Storage(const Storage &n) { }

#ifdef SQLITE_SUPPORT
        SQLiteWrapper sqlite;              /**< Database */
#endif
        std::vector<Account*> accounts;    /**< Loaded account data */
        std::vector<Being*> characters;    /**< Loaded account data */

    public:
        /**
         * Constructor.
         */
        Storage();

        /**
         * Destructor.
         */
        ~Storage();

        /**
         * Create tables if master is empty
         */
        void create_tables_if_necessary();

        /**
         * Save changes to database
         */
        void save();

        /**
         * Account count (test function)
         */
        unsigned int accountCount();

        /**
         * Get account & associated data
         */
        Account* getAccount(const std::string &username);

        /**
         * Get character of username
         */
        Being* getCharacter(const std::string &username);
};

#endif /* STORAGE_H */

