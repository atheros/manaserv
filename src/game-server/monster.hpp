/*
 *  The Mana World Server
 *  Copyright 2004 The Mana World Development Team
 *
 *  This file is part of The Mana World.
 *
 *  The Mana World  is free software; you can redistribute  it and/or modify it
 *  under the terms of the GNU General  Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or any later version.
 *
 *  The Mana  World is  distributed in  the hope  that it  will be  useful, but
 *  WITHOUT ANY WARRANTY; without even  the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 *  more details.
 *
 *  You should  have received a  copy of the  GNU General Public  License along
 *  with The Mana  World; if not, write to the  Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *  $Id: controller.h 3191 2007-03-15 23:47:13Z crush_tmw $
 */

#ifndef _TMWSERV_CONTROLLER_H_
#define _TMWSERV_CONTROLLER_H_

#include "game-server/being.hpp"

/**
 * The class for a fightable monster with its own AI
 */
class Monster: public Being
{
    public:
        /**
         * Constructor.
         */
        Monster();

        /**
         * Performs one step of controller logic.
         */
        void update();

        /**
         * Kills the being
         */
        virtual void die();

    protected:
        /**
         * Gets the stats of the currently equipped weapon that are relevant
         * for damage calculation
         */
        virtual WeaponStats getWeaponStats();

        /**
         * Calculates all derived attributes
         */
        void calculateDerivedAttributes();

    private:
        /** Count down till next random movement (temporary). */
        unsigned int mCountDown;
};

#endif // _TMWSERV_CONTROLLER_H_
/*
 *  The Mana World Server
 *  Copyright 2004 The Mana World Development Team
 *
 *  This file is part of The Mana World.
 *
 *  The Mana World  is free software; you can redistribute  it and/or modify it
 *  under the terms of the GNU General  Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or any later version.
 *
 *  The Mana  World is  distributed in  the hope  that it  will be  useful, but
 *  WITHOUT ANY WARRANTY; without even  the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 *  more details.
 *
 *  You should  have received a  copy of the  GNU General Public  License along
 *  with The Mana  World; if not, write to the  Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *  $Id: controller.h 3191 2007-03-15 23:47:13Z crush_tmw $
 */

#ifndef _TMWSERV_CONTROLLER_H_
#define _TMWSERV_CONTROLLER_H_

#include "game-server/being.hpp"

#include "utils/safeptr.hpp"

/**
 * The class for a fightable monster with its own AI
 */
class Monster: public Being
{
    public:
        /**
         * Constructor.
         */
        Monster();

        /**
         * Performs one step of controller logic.
         */
        void update();

        /**
         * Kills the being
         */
        virtual void die();

        /**
         *
         */
        virtual int damage(Damage);

    protected:
        /**
         * Gets the stats of the currently equipped weapon that are relevant
         * for damage calculation
         */
        virtual WeaponStats getWeaponStats();

        /**
         * Calculates all derived attributes
         */
        void calculateDerivedAttributes();

    private:
        /** Count down till next random movement (temporary). */
        unsigned int mCountDown;

        /** List of targets and how much priority they have */
        struct BattleLogEntry
        {
            int timestamp;
            SafePtr attacker;
            int skill;
            int value;
        };
        std::list<BattleLogEntry> battleLog;
};

#endif // _TMWSERV_CONTROLLER_H_