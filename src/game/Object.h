/*
 * Copyright (C) 2005-2012 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _OBJECT_H
#define _OBJECT_H

#include "Common.h"
#include "ByteBuffer.h"
#include "UpdateFieldFlags.h"
#include "UpdateData.h"
#include "ObjectGuid.h"
#include "Camera.h"
#include "ObjectLock.h"
#include "SharedDefines.h"
#include "WorldObjectEvents.h"
#include "WorldLocation.h"
#include "LootMgr.h"
#include "Util.h"

#include <set>
#include <string>

#define DEFAULT_WORLD_OBJECT_SIZE   0.388999998569489f      // currently used (correctly?) for any non Unit world objects. This is actually the bounding_radius, like player/creature from creature_model_data
#define DEFAULT_OBJECT_SCALE        1.0f                    // player/item scale as default, npc/go from database, pets from dbc

#define MAX_STEALTH_DETECT_RANGE    45.0f

#define DEFAULT_HIDDEN_MODEL_ID     11686                   // common "hidden" (invisible) model ID

enum TempSummonType
{
    TEMPSUMMON_MANUAL_DESPAWN              = 0,             // despawns when UnSummon() is called
    TEMPSUMMON_DEAD_DESPAWN                = 1,             // despawns when the creature disappears
    TEMPSUMMON_CORPSE_DESPAWN              = 2,             // despawns instantly after death
    TEMPSUMMON_CORPSE_TIMED_DESPAWN        = 3,             // despawns after a specified time after death (or when the creature disappears)
    TEMPSUMMON_TIMED_DESPAWN               = 4,             // despawns after a specified time
    TEMPSUMMON_TIMED_OOC_DESPAWN           = 5,             // despawns after a specified time after the creature is out of combat
    TEMPSUMMON_TIMED_OR_DEAD_DESPAWN       = 6,             // despawns after a specified time OR when the creature disappears
    TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN     = 7,             // despawns after a specified time OR when the creature dies
    TEMPSUMMON_TIMED_OOC_OR_DEAD_DESPAWN   = 8,             // despawns after a specified time (OOC) OR when the creature disappears
    TEMPSUMMON_TIMED_OOC_OR_CORPSE_DESPAWN = 9,             // despawns after a specified time (OOC) OR when the creature dies

    // Place for future despawn types
    TEMPSUMMON_LOST_OWNER_DESPAWN                           = 20,            // despawns when creature lost charmer/owner
    TEMPSUMMON_DEAD_OR_LOST_OWNER_DESPAWN                   = 21,            // despawns when creature lost charmer/owner
    TEMPSUMMON_TIMED_OR_DEAD_OR_LOST_OWNER_DESPAWN          = 22,            // despawns when creature lost charmer/owner, or by time
    TEMPSUMMON_TIMED_OR_DEAD_OR_LOST_UNIQUENESS_DESPAWN     = 23,            // despawns when owner spawn creature this type in visible range, or by rules of TEMPSUMMON_TIMED_OR_DEAD_DESPAWN
    TEMPSUMMON_DEAD_OR_LOST_UNIQUENESS_DESPAWN              = 24,            // despawns when owner spawn creature this type in visible range, or by rules of TEMPSUMMON_DEAD_DESPAWN
};

class WorldPacket;
class UpdateData;
class WorldSession;
class Creature;
class GameObject;
class Player;
class Group;
class Unit;
class Group;
class Map;
class UpdateMask;
class InstanceData;
class TerrainInfo;
class Transport;
class TransportBase;
class TransportInfo;
struct MangosStringLocale;

typedef UNORDERED_MAP<ObjectGuid, UpdateData> UpdateDataMapType;

//use this class to measure time between world update ticks
//essential for units updating their spells after cells become active
class WorldUpdateCounter
{
    public:
        WorldUpdateCounter() : m_tmStart(0) {}

        time_t timeElapsed()
        {
            if(!m_tmStart)
                m_tmStart = WorldTimer::tickPrevTime();

            return WorldTimer::getMSTimeDiff(m_tmStart, WorldTimer::tickTime());
        }

        void Reset() { m_tmStart = WorldTimer::tickTime(); }

    private:
        uint32 m_tmStart;
};

class Object;
struct UpdateFieldData
{
    public:
        UpdateFieldData(Object const* object, Player* target);
        bool IsUpdateNeeded(uint16 fieldIndex, uint32 fieldNotifyFlags) const { return HasFlags(fieldIndex, fieldNotifyFlags) || (HasFlags(fieldIndex, UF_FLAG_SPECIAL_INFO) && m_hasSpecialInfo); }
        bool IsUpdateFieldVisible(uint16 fieldIndex) const;
    private:
        inline bool HasFlags(uint16 fieldIndex, uint32 flags) const { return m_flags[fieldIndex] & flags; }

        uint32* m_flags;
        bool m_isSelf;
        bool m_isOwner;
        bool m_isItemOwner;
        bool m_hasSpecialInfo;
        bool m_isPartyMember;
};

class MANGOS_DLL_SPEC Object
{
    public:
        virtual ~Object();

        const bool& IsInWorld() const { return m_inWorld; }
        virtual void AddToWorld()
        {
            if(m_inWorld)
                return;

            m_inWorld = true;

            // synchronize values mirror with values array (changes will send in updatecreate opcode any way
            ClearUpdateMask(false);                         // false - we can't have update data in update queue before adding to world
        }
        virtual void RemoveFromWorld(bool /*remove*/)
        {
            // if we remove from world then sending changes not required
            ClearUpdateMask(true);
            m_inWorld = false;
        }
        bool IsInitialized() const { return (m_uint32Values ? true : false);}

        ObjectGuid const& GetObjectGuid() const { return GetGuidValue(OBJECT_FIELD_GUID); }
        uint32 GetGUIDLow() const { return GetObjectGuid().GetCounter(); }
        PackedGuid const& GetPackGUID() const { return m_PackGUID; }
        std::string GetGuidStr() const { return GetObjectGuid().GetString(); }

        uint32 GetEntry() const { return GetUInt32Value(OBJECT_FIELD_ENTRY); }
        void SetEntry(uint32 entry) { SetUInt32Value(OBJECT_FIELD_ENTRY, entry); }

        float GetObjectScale() const
        {
            return m_floatValues[OBJECT_FIELD_SCALE_X] ? m_floatValues[OBJECT_FIELD_SCALE_X] : DEFAULT_OBJECT_SCALE;
        }

        void SetObjectScale(float newScale);

        uint8 GetTypeId() const { return m_objectTypeId; }
        bool isType(TypeMask mask) const { return (mask & m_objectType); }

        virtual void BuildCreateUpdateBlockForPlayer( UpdateData *data, Player *target ) const;
        void SendCreateUpdateToPlayer(Player* player);

        // must be overwrite in appropriate subclasses (WorldObject, Item currently), or will crash
        virtual void AddToClientUpdateList();
        virtual void RemoveFromClientUpdateList();
        virtual void BuildUpdateData(UpdateDataMapType& update_players);
        void MarkForClientUpdate();
        void SendForcedObjectUpdate();

        virtual GuidSet const* GetObjectsUpdateQueue() { return NULL; };
        bool IsMarkedForClientUpdate() const { return m_objectUpdated; };
        virtual Object* GetDependentObject(ObjectGuid const& guid) { return NULL; };
        virtual void RemoveUpdateObject(ObjectGuid const& guid) {};
        virtual void AddUpdateObject(ObjectGuid const& guid) {};

        void SetFieldNotifyFlag(uint16 flag) { m_fieldNotifyFlags |= flag; }
        void RemoveFieldNotifyFlag(uint16 flag) { m_fieldNotifyFlags &= ~flag; }

        void BuildValuesUpdateBlockForPlayer( UpdateData *data, Player *target ) const;
        void BuildOutOfRangeUpdateBlock( UpdateData *data ) const;
        void BuildMovementUpdateBlock( UpdateData * data, uint16 flags = 0 ) const;

        virtual void DestroyForPlayer( Player *target, bool anim = false ) const;

        const int32& GetInt32Value( uint16 index ) const
        {
            MANGOS_ASSERT( index < m_valuesCount || PrintIndexError( index , false) );
            return m_int32Values[ index ];
        }

        const uint32& GetUInt32Value( uint16 index ) const
        {
            MANGOS_ASSERT( index < m_valuesCount || PrintIndexError( index , false) );
            return m_uint32Values[ index ];
        }

        const uint64& GetUInt64Value( uint16 index ) const
        {
            MANGOS_ASSERT( index + 1 < m_valuesCount || PrintIndexError( index , false) );
            return *((uint64*)&(m_uint32Values[ index ]));
        }

        const float& GetFloatValue( uint16 index ) const
        {
            MANGOS_ASSERT( index < m_valuesCount || PrintIndexError( index , false ) );
            return m_floatValues[ index ];
        }

        uint8 GetByteValue( uint16 index, uint8 offset) const
        {
            MANGOS_ASSERT( index < m_valuesCount || PrintIndexError( index , false) );
            MANGOS_ASSERT( offset < 4 );
            return *(((uint8*)&m_uint32Values[ index ])+offset);
        }

        uint16 GetUInt16Value( uint16 index, uint8 offset) const
        {
            MANGOS_ASSERT( index < m_valuesCount || PrintIndexError( index , false) );
            MANGOS_ASSERT( offset < 2 );
            return *(((uint16*)&m_uint32Values[ index ])+offset);
        }

        ObjectGuid const& GetGuidValue( uint16 index ) const { return *reinterpret_cast<ObjectGuid const*>(&GetUInt64Value(index)); }

        void SetInt32Value(  uint16 index,        int32  value );
        void SetUInt32Value( uint16 index,       uint32  value );
        void SetUInt64Value( uint16 index, const uint64 &value );
        void SetFloatValue(  uint16 index,       float   value );
        void SetByteValue(   uint16 index, uint8 offset, uint8 value );
        void SetUInt16Value( uint16 index, uint8 offset, uint16 value );
        void SetInt16Value(  uint16 index, uint8 offset, int16 value ) { SetUInt16Value(index,offset,(uint16)value); }
        void SetGuidValue( uint16 index, ObjectGuid const& value ) { SetUInt64Value(index, value.GetRawValue()); }
        void SetStatFloatValue( uint16 index, float value);
        void SetStatInt32Value( uint16 index, int32 value);

        void ApplyModUInt32Value(uint16 index, int32 val, bool apply);
        void ApplyModInt32Value(uint16 index, int32 val, bool apply);
        void ApplyModUInt64Value(uint16 index, int32 val, bool apply);
        void ApplyModPositiveFloatValue( uint16 index, float val, bool apply);
        void ApplyModSignedFloatValue( uint16 index, float val, bool apply);

        void ApplyPercentModFloatValue(uint16 index, float val, bool apply)
        {
            float var = GetFloatValue(index);
            ApplyPercentModFloatVar(var, val, apply);
            SetFloatValue(index, var);
        }

        void SetFlag( uint16 index, uint32 newFlag );
        void RemoveFlag( uint16 index, uint32 oldFlag );

        void ToggleFlag( uint16 index, uint32 flag)
        {
            if(HasFlag(index, flag))
                RemoveFlag(index, flag);
            else
                SetFlag(index, flag);
        }

        bool HasFlag( uint16 index, uint32 flag ) const
        {
            MANGOS_ASSERT( index < m_valuesCount || PrintIndexError( index , false ) );
            return (m_uint32Values[ index ] & flag) != 0;
        }

        void ApplyModFlag( uint16 index, uint32 flag, bool apply)
        {
            if (apply)
                SetFlag(index, flag);
            else
                RemoveFlag(index, flag);
        }

        void SetByteFlag( uint16 index, uint8 offset, uint8 newFlag );
        void RemoveByteFlag( uint16 index, uint8 offset, uint8 newFlag );

        void ToggleByteFlag( uint16 index, uint8 offset, uint8 flag )
        {
            if (HasByteFlag(index, offset, flag))
                RemoveByteFlag(index, offset, flag);
            else
                SetByteFlag(index, offset, flag);
        }

        bool HasByteFlag( uint16 index, uint8 offset, uint8 flag ) const
        {
            MANGOS_ASSERT( index < m_valuesCount || PrintIndexError( index , false ) );
            MANGOS_ASSERT( offset < 4 );
            return (((uint8*)&m_uint32Values[index])[offset] & flag) != 0;
        }

        void ApplyModByteFlag( uint16 index, uint8 offset, uint32 flag, bool apply)
        {
            if (apply)
                SetByteFlag(index, offset, flag);
            else
                RemoveByteFlag(index, offset, flag);
        }

        void SetShortFlag(uint16 index, bool highpart, uint16 newFlag);
        void RemoveShortFlag(uint16 index, bool highpart, uint16 oldFlag);

        void ToggleShortFlag( uint16 index, bool highpart, uint8 flag )
        {
            if (HasShortFlag(index, highpart, flag))
                RemoveShortFlag(index, highpart, flag);
            else
                SetShortFlag(index, highpart, flag);
        }

        bool HasShortFlag( uint16 index, bool highpart, uint8 flag ) const
        {
            MANGOS_ASSERT( index < m_valuesCount || PrintIndexError( index , false ) );
            return (((uint16*)&m_uint32Values[index])[highpart ? 1 : 0] & flag) != 0;
        }

        void ApplyModShortFlag( uint16 index, bool highpart, uint32 flag, bool apply)
        {
            if (apply)
                SetShortFlag(index, highpart, flag);
            else
                RemoveShortFlag(index, highpart, flag);
        }

        void SetFlag64( uint16 index, uint64 newFlag )
        {
            uint64 oldval = GetUInt64Value(index);
            uint64 newval = oldval | newFlag;
            SetUInt64Value(index,newval);
        }

        void RemoveFlag64( uint16 index, uint64 oldFlag )
        {
            uint64 oldval = GetUInt64Value(index);
            uint64 newval = oldval & ~oldFlag;
            SetUInt64Value(index,newval);
        }

        void ToggleFlag64( uint16 index, uint64 flag)
        {
            if (HasFlag64(index, flag))
                RemoveFlag64(index, flag);
            else
                SetFlag64(index, flag);
        }

        bool HasFlag64( uint16 index, uint64 flag ) const
        {
            MANGOS_ASSERT( index < m_valuesCount || PrintIndexError( index , false ) );
            return (GetUInt64Value( index ) & flag) != 0;
        }

        void ApplyModFlag64( uint16 index, uint64 flag, bool apply)
        {
            if (apply)
                SetFlag64(index, flag);
            else
                RemoveFlag64(index, flag);
        }

        void ClearUpdateMask(bool remove);

        bool LoadValues(const char* data);

        uint16 GetValuesCount() const { return m_valuesCount; }

        // Frozen Mod
        void ForceValuesUpdateAtIndex(uint16);
        // Frozen Mod

        virtual bool HasQuest(uint32 /* quest_id */) const { return false; }
        virtual bool HasInvolvedQuest(uint32 /* quest_id */) const { return false; }

    protected:
        Object();

        void _InitValues();
        void _Create(uint32 guidlow, uint32 entry, HighGuid guidhigh) { _Create(ObjectGuid(guidhigh, entry, guidlow)); }
        void _Create(ObjectGuid guid);

        void _SetUpdateBits(UpdateMask* updateMask, Player* target) const;
        void _SetCreateBits(UpdateMask* updateMask, Player* target) const;

        void BuildMovementUpdate(ByteBuffer * data, uint16 updateFlags) const;
        void BuildValuesUpdate(uint8 updatetype, ByteBuffer *data, UpdateMask *updateMask, Player *target ) const;
        void BuildUpdateDataForPlayer(Player* pl, UpdateDataMapType& update_players);

        uint16 m_objectType;

        uint8 m_objectTypeId;
        uint16 m_updateFlag;

        union
        {
            int32  *m_int32Values;
            uint32 *m_uint32Values;
            float  *m_floatValues;
        };

        std::vector<bool> m_changedValues;

        uint16 m_valuesCount;
        uint16 m_fieldNotifyFlags;

        bool m_objectUpdated;

    private:
        bool m_inWorld;

        PackedGuid m_PackGUID;

        Object(const Object&);                              // prevent generation copy constructor
        Object& operator=(Object const&);                   // prevent generation assigment operator

    public:
        // for output helpfull error messages from ASSERTs
        bool PrintIndexError(uint32 index, bool set) const;
        bool PrintEntryError(char const* descr) const;
};

struct WorldObjectChangeAccumulator;

namespace Movement
{
    class MoveSpline;
};

class MANGOS_DLL_SPEC WorldObject : public Object
{
    friend struct WorldObjectChangeAccumulator;

    public:

        //class is used to manipulate with WorldUpdateCounter
        //it is needed in order to get time diff between two object's Update() calls
        class MANGOS_DLL_SPEC UpdateHelper
        {
            public:
                explicit UpdateHelper(WorldObject * obj) : m_obj(obj) {}
                ~UpdateHelper() { }

                void Update( uint32 time_diff )
                {
                    m_obj->Update( m_obj->m_updateTracker.timeElapsed(), time_diff);
                    m_obj->m_updateTracker.Reset();
                }

            private:
                UpdateHelper( const UpdateHelper& );
                UpdateHelper& operator=( const UpdateHelper& );

                WorldObject * const m_obj;
        };

        virtual ~WorldObject();

        virtual void Update(uint32 /*update_diff*/, uint32 /*time_diff*/) {}

        void _Create(ObjectGuid guid, uint32 phaseMask);

        void AddToWorld();
        virtual void RemoveFromWorld(bool remove) override;

        TransportInfo* GetTransportInfo() const { return m_transportInfo; }
        bool IsBoarded() const { return m_transportInfo != NULL; }
        void SetTransportInfo(TransportInfo* transportInfo) { m_transportInfo = transportInfo; }

        virtual bool IsTransport() const { return false; };
        TransportBase* GetTransportBase();

        void Relocate(WorldLocation const& location);
        void Relocate(Position const& position);
        void SetOrientation(float orientation);

        // FIXME - need remove wrapper after cleanup SD2
        void Relocate(float x, float y, float z, float orientation = 0.0f) { Relocate(Position(x, y, z, orientation, GetPhaseMask())); };

        float const& GetPositionX() const     { return m_position.x; }
        float const& GetPositionY() const     { return m_position.y; }
        float const& GetPositionZ() const     { return m_position.z; }
        float const& GetOrientation() const   { return m_position.orientation; }
        void GetPosition(float &x, float &y, float &z ) const { x = m_position.x; y = m_position.y; z = m_position.z; }
        WorldLocation const& GetPosition() const { return m_position; };

        virtual bool IsOnTransport() const;
        virtual Transport* GetTransport() const;
        float GetTransOffsetX() const { return m_position.GetTransportPos().getX(); }
        float GetTransOffsetY() const { return m_position.GetTransportPos().getY(); }
        float GetTransOffsetZ() const { return m_position.GetTransportPos().getZ(); }
        float GetTransOffsetO() const { return m_position.GetTransportPos().getO(); }
        Position const& GetTransportPosition() const { return m_position.GetTransportPos(); };
        void SetTransportPosition(Position const& pos) { m_position.SetTransportPosition(pos); };
        void ClearTransportData() { m_position.ClearTransportData(); };

        void GetNearPoint2D( float &x, float &y, float distance, float absAngle) const;
        void GetNearPoint(WorldObject const* searcher, float &x, float &y, float &z, float searcher_bounding_radius, float distance2d, float absAngle) const;
        void GetClosePoint(float &x, float &y, float &z, float bounding_radius, float distance2d = 0.0f, float angle = 0.0f, WorldObject const* obj = NULL) const
        {
            // angle calculated from current orientation
            GetNearPoint(obj, x, y, z, bounding_radius, distance2d, GetOrientation() + angle);
        }

        WorldLocation GetClosePoint(float bounding_radius, float distance2d = 0.0f, float angle = 0.0f, WorldObject const* obj = NULL)
        {
            WorldLocation loc = GetPosition();
            GetNearPoint(obj, loc.x, loc.y, loc.z, bounding_radius, distance2d, loc.o + angle);
            return loc;
        }

        void GetContactPoint( const WorldObject* obj, float &x, float &y, float &z, float distance2d = CONTACT_DISTANCE) const
        {
            // angle to face `obj` to `this` using distance includes size of `obj`
            GetNearPoint(obj, x, y, z, obj->GetObjectBoundingRadius(), distance2d, GetAngle(obj));
        }

        virtual float GetObjectBoundingRadius() const { return DEFAULT_WORLD_OBJECT_SIZE; }

        bool IsPositionValid() const;
        void UpdateGroundPositionZ(float x, float y, float &z) const;
        void UpdateAllowedPositionZ(float x, float y, float &z) const;

        void GetRandomPoint( float x, float y, float z, float distance, float &rand_x, float &rand_y, float &rand_z ) const;

        uint32 GetMapId() const { return m_position.GetMapId(); }
        uint32 GetInstanceId() const { return m_position.GetInstanceId(); }

        virtual void SetPhaseMask(uint32 newPhaseMask, bool update);
        uint32 GetPhaseMask() const { return m_position.GetPhaseMask(); }
        bool InSamePhase(WorldObject const* obj) const { return InSamePhase(obj->GetPhaseMask()); }
        bool InSamePhase(uint32 phasemask) const { return (GetPhaseMask() & phasemask); }

        uint32 GetZoneId() const;
        uint32 GetAreaId() const;
        void GetZoneAndAreaId(uint32& zoneid, uint32& areaid) const;

        InstanceData* GetInstanceData() const;

        const char* GetName() const { return m_name.c_str(); }
        void SetName(const std::string& newname) { m_name=newname; }

        virtual const char* GetNameForLocaleIdx(int32 /*locale_idx*/) const { return GetName(); }

        float GetDistance(WorldLocation const& loc) const;

        float GetDistance( const WorldObject* obj ) const;
        float GetDistance(float x, float y, float z) const;
        float GetDistance2d(const WorldObject* obj) const;
        float GetDistance2d(float x, float y) const;
        float GetDistanceZ(const WorldObject* obj) const;
        bool IsInMap(const WorldObject* obj) const
        {
            return IsInWorld() && obj->IsInWorld() && (GetMap() == obj->GetMap()) && InSamePhase(obj);
        }
        bool IsWithinDist3d(float x, float y, float z, float dist2compare) const;
        bool IsWithinDist3d(Location const& loc, float dist2compare) const;
        bool IsWithinDist2d(float x, float y, float dist2compare) const;
        bool _IsWithinDist(WorldObject const* obj, float dist2compare, bool is3D) const;

        // use only if you will sure about placing both object at same map
        bool IsWithinDist(WorldObject const* obj, float dist2compare, bool is3D = true) const
        {
            return obj && _IsWithinDist(obj,dist2compare,is3D);
        }

        bool IsWithinDistInMap(WorldObject const* obj, float dist2compare, bool is3D = true) const
        {
            return obj && IsInMap(obj) && _IsWithinDist(obj,dist2compare,is3D);
        }
        bool IsWithinLOS(float x, float y, float z) const;
        bool IsWithinLOSInMap(const WorldObject* obj) const;
        bool GetDistanceOrder(WorldObject const* obj1, WorldObject const* obj2, bool is3D = true) const;
        bool IsInRange(WorldObject const* obj, float minRange, float maxRange, bool is3D = true) const;
        bool IsInRange2d(float x, float y, float minRange, float maxRange) const;
        bool IsInRange3d(float x, float y, float z, float minRange, float maxRange) const;

        bool IsInBetween(const WorldObject *obj1, const WorldObject *obj2, float size = 0) const;
        float GetExactDist2dSq(float x, float y) const
        { float dx = m_position.x - x; float dy = m_position.y - y; return dx*dx + dy*dy; }
        float GetExactDist2d(const float x, const float y) const
        { return sqrt(GetExactDist2dSq(x, y)); }

        float GetAngle( const WorldObject* obj ) const;
        float GetAngle( const float x, const float y ) const;
        bool HasInArc( const float arcangle, const WorldObject* obj ) const;
        bool isInFrontInMap(WorldObject const* target,float distance, float arc = M_PI) const;
        bool isInBackInMap(WorldObject const* target, float distance, float arc = M_PI) const;
        bool isInFront(WorldObject const* target,float distance, float arc = M_PI) const;
        bool isInBack(WorldObject const* target, float distance, float arc = M_PI) const;

        virtual void CleanupsBeforeDelete();                   // used in destructor or explicitly before mass creature delete to remove cross-references to already deleted units

        virtual void SendMessageToSet(WorldPacket* data, bool self) const;
        virtual void SendMessageToSetInRange(WorldPacket* data, float dist, bool self) const;
        void SendMessageToSetExcept(WorldPacket* data, Player const* skipped_receiver) const;

        void MonsterSay(const char* text, uint32 language, Unit const* target = NULL) const;
        void MonsterYell(const char* text, uint32 language, Unit const* target = NULL) const;
        void MonsterTextEmote(const char* text, Unit const* target, bool IsBossEmote = false) const;
        void MonsterWhisper(const char* text, Unit const* target, bool IsBossWhisper = false) const;
        void MonsterText(MangosStringLocale const* textData, Unit const* target) const;
        static void BuildMonsterChat(WorldPacket* data, ObjectGuid senderGuid, uint8 msgtype, char const* text, uint32 language, char const* name, ObjectGuid targetGuid, char const* targetName);

        void PlayDistanceSound(uint32 sound_id, Player const* target = NULL) const;
        void PlayDirectSound(uint32 sound_id, Player const* target = NULL) const;

        void SendObjectDeSpawnAnim(ObjectGuid guid);
        void SendGameObjectCustomAnim(ObjectGuid guid, uint32 animId = 0);

        virtual bool IsHostileTo(Unit const* unit) const =0;
        virtual bool IsFriendlyTo(Unit const* unit) const =0;
        bool IsControlledByPlayer() const;

        virtual void SaveRespawnTime() {}
        void AddObjectToRemoveList();
        void RemoveObjectFromRemoveList();

        void UpdateObjectVisibility();
        virtual void UpdateVisibilityAndView();             // update visibility for object and object for all around

        // main visibility check function in normal case (ignore grey zone distance check)
        bool isVisibleFor(Player const* u, WorldObject const* viewPoint) const { return isVisibleForInState(u,viewPoint,false); }

        // low level function for visibility change code, must be define in all main world object subclasses
        virtual bool isVisibleForInState(Player const* u, WorldObject const* viewPoint, bool inVisibleList) const = 0;

        void SetMap(Map * map);
        Map * GetMap() const { return m_currMap; }
        //used to check all object's GetMap() calls when object is not in world!
        void ResetMap() { m_currMap = NULL; }

        ObjectLockType& GetLock(MapLockType _locktype = MAP_LOCK_TYPE_DEFAULT);

        //obtain terrain data for map where this object belong...
        TerrainInfo const* GetTerrain() const;

        void AddToClientUpdateList() override;
        void RemoveFromClientUpdateList() override;
        void BuildUpdateData(UpdateDataMapType &) override;

        Creature* SummonCreature(uint32 id, float x, float y, float z, float ang, TempSummonType spwtype, uint32 despwtime, bool asActiveObject = false);
        Creature* SummonCreature(uint32 id, TempSummonType spwType, uint32 despwTime, bool asActiveObject = false)
        {
            return SummonCreature(id, 0.0f, 0.0f, 0.0f, 0.0f, spwType, despwTime, asActiveObject);
        }

        GameObject* SummonGameobject(uint32 id, float x, float y, float z, float angle, uint32 despwtime);

        // Loot System
        Loot loot;

        void StartGroupLoot(Group* group, uint32 timer);
        void StopGroupLoot();
        ObjectGuid GetLootRecipientGuid() const { return m_lootRecipientGuid; }
        uint32 GetLootGroupRecipientId() const { return m_lootGroupRecipientId; }
        Player* GetLootRecipient() const;                   // use group cases as prefered
        Group* GetGroupLootRecipient() const;
        bool HasLootRecipient() const { return m_lootGroupRecipientId || !m_lootRecipientGuid.IsEmpty(); }
        bool IsGroupLootRecipient() const { return m_lootGroupRecipientId; }
        void SetLootRecipient(Unit* unit);
        Player* GetOriginalLootRecipient() const;           // ignore group changes/etc, not for looting

        // helper functions to select units
        Creature* GetClosestCreatureWithEntry(WorldObject* pSource, uint32 uiEntry, float fMaxSearchRange);
        GameObject* GetClosestGameObjectWithEntry(const WorldObject* pSource, uint32 uiEntry, float fMaxSearchRange);
        void GetGameObjectListWithEntryInGrid(std::list<GameObject*>& lList, uint32 uiEntry, float fMaxSearchRange);

        bool isActiveObject() const { return m_isActiveObject || m_viewPoint.hasViewers(); }
        void SetActiveObjectState(bool active);

        uint32 GetLastUpdateTime() const { return m_LastUpdateTime; }
        void SetLastUpdateTime() { m_LastUpdateTime = WorldTimer::getMSTime(); }

        ViewPoint& GetViewPoint() { return m_viewPoint; }

        // ASSERT print helper
        bool PrintCoordinatesError(float x, float y, float z, char const* descr) const;

        // WorldState operations
        void UpdateWorldState(uint32 state, uint32 value);
        uint32 GetWorldState(uint32 state);

        // Event handler
        WorldObjectEventProcessor* GetEvents();
        void UpdateEvents(uint32 update_diff, uint32 time);
        void KillAllEvents(bool force);
        void AddEvent(BasicEvent* Event, uint64 e_time, bool set_addtime = true);

        virtual bool IsVehicle() const { return false; }

        // Movement
        Movement::MoveSpline* movespline;
        ShortTimeTracker m_movesplineTimer;

        // Visibility operations for object in (must be used only in active state!)
        GuidSet& GetNotifiedClients() { return m_notifiedClients;};
        void  AddNotifiedClient(ObjectGuid const& guid)    { m_notifiedClients.insert(guid); };
        void  RemoveNotifiedClient(ObjectGuid const& guid) { m_notifiedClients.erase(guid); };
        bool  HasNotifiedClients() const { return !m_notifiedClients.empty(); };

    protected:
        explicit WorldObject();

        //these functions are used mostly for Relocate() and Corpse/Player specific stuff...
        //use them ONLY in LoadFromDB()/Create() funcs and nowhere else!
        //mapId/instanceId should be set in SetMap() function!
        void SetLocationMapId(uint32 _mapId)           { m_position.SetMapId(_mapId); }
        void SetLocationInstanceId(uint32 _instanceId) { m_position.SetInstanceId(_instanceId); }

        uint32 m_groupLootTimer;                            // (msecs)timer used for group loot
        uint32 m_groupLootId;                               // used to find group which is looting corpse

        ObjectGuid m_lootRecipientGuid;                     // player who will have rights for looting if m_lootGroupRecipient==0 or group disbanded
        uint32 m_lootGroupRecipientId;                      // group who will have rights for looting if set and exist

        std::string m_name;

        TransportInfo* m_transportInfo;

    private:
        Map* m_currMap;                                     //current object's Map location

        WorldLocation m_position;                           // Contains all needed coords for object

        ViewPoint m_viewPoint;
        WorldUpdateCounter m_updateTracker;
        bool m_isActiveObject;

        uint32 m_LastUpdateTime;

        WorldObjectEventProcessor m_Events;

        GuidSet    m_notifiedClients;
};

#endif
