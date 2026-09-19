////////////////////////////////////////////////////////////////////////////
//	Module 		: smart_cast.h
//	Created 	: 17.09.2004
//  Modified 	: 17.09.2004
//	Author		: Dmitriy Iassenev
//	Description : Smart dynamic cast
////////////////////////////////////////////////////////////////////////////

// WARNING: Due to conditional include of different implementations, this file CAN NOT have a
// #pragma once include guard. It can however have a marco-declared include guard.
#ifndef SMART_CAST_H
#define SMART_CAST_H

#ifdef DEBUG
#ifndef PURE_DYNAMIC_CAST
#define PURE_DYNAMIC_CAST
#endif // PURE_DYNAMIC_CAST
#endif // DEBUG

#ifdef PURE_DYNAMIC_CAST
#ifndef SMART_CAST_HELPER_DEFINED
#define SMART_CAST_HELPER_DEFINED
#include <type_traits>
#include "Include/xrRender/RenderVisual.h"
#include "Include/xrRender/Kinematics.h"
#include "Include/xrRender/KinematicsAnimated.h"
#include "Include/xrRender/ParticleCustom.h"

template <typename Target, typename Source, typename Enable = void>
struct smart_cast_helper
{
    static inline Target cast(Source* source)
    {
        return dynamic_cast<Target>(source);
    }
    static inline Target cast(Source& source)
    {
        return dynamic_cast<Target>(source);
    }
};

// Specialization for Target = IKinematics* from any IRenderVisual-derived pointer
template <typename Target, typename Source>
struct smart_cast_helper<Target, Source, 
    std::enable_if_t<
        (std::is_same_v<Target, IKinematics*> || std::is_same_v<Target, const IKinematics*>) &&
        std::is_base_of_v<IRenderVisual, Source>
    >>
{
    static inline Target cast(Source* source)
    {
        return source ? static_cast<Target>(const_cast<IRenderVisual*>(static_cast<const IRenderVisual*>(source))->dcast_PKinematics()) : nullptr;
    }
    static inline Target cast(Source& source)
    {
        return static_cast<Target>(const_cast<IRenderVisual*>(static_cast<const IRenderVisual*>(&source))->dcast_PKinematics());
    }
};

// Specialization for Target = IKinematics& from any IRenderVisual-derived object
template <typename Target, typename Source>
struct smart_cast_helper<Target, Source, 
    std::enable_if_t<
        (std::is_same_v<Target, IKinematics&> || std::is_same_v<Target, const IKinematics&>) &&
        std::is_base_of_v<IRenderVisual, Source>
    >>
{
    static inline Target cast(Source& source)
    {
        auto* k = const_cast<IRenderVisual*>(static_cast<const IRenderVisual*>(&source))->dcast_PKinematics();
        return *static_cast<std::remove_reference_t<Target>*>(k);
    }
};

// Specialization for Target = IKinematicsAnimated* from any IRenderVisual-derived pointer
template <typename Target, typename Source>
struct smart_cast_helper<Target, Source,
    std::enable_if_t<
        (std::is_same_v<Target, IKinematicsAnimated*> || std::is_same_v<Target, const IKinematicsAnimated*>) &&
        std::is_base_of_v<IRenderVisual, Source>
    >>
{
    static inline Target cast(Source* source)
    {
        return source ? static_cast<Target>(const_cast<IRenderVisual*>(static_cast<const IRenderVisual*>(source))->dcast_PKinematicsAnimated()) : nullptr;
    }
    static inline Target cast(Source& source)
    {
        return static_cast<Target>(const_cast<IRenderVisual*>(static_cast<const IRenderVisual*>(&source))->dcast_PKinematicsAnimated());
    }
};

// Specialization for Target = IKinematicsAnimated& from any IRenderVisual-derived object
template <typename Target, typename Source>
struct smart_cast_helper<Target, Source,
    std::enable_if_t<
        (std::is_same_v<Target, IKinematicsAnimated&> || std::is_same_v<Target, const IKinematicsAnimated&>) &&
        std::is_base_of_v<IRenderVisual, Source>
    >>
{
    static inline Target cast(Source& source)
    {
        auto* k = const_cast<IRenderVisual*>(static_cast<const IRenderVisual*>(&source))->dcast_PKinematicsAnimated();
        return *static_cast<std::remove_reference_t<Target>*>(k);
    }
};

// Specialization for Target = IParticleCustom* from any IRenderVisual-derived pointer
template <typename Target, typename Source>
struct smart_cast_helper<Target, Source,
    std::enable_if_t<
        (std::is_same_v<Target, IParticleCustom*> || std::is_same_v<Target, const IParticleCustom*>) &&
        std::is_base_of_v<IRenderVisual, Source>
    >>
{
    static inline Target cast(Source* source)
    {
        return source ? static_cast<Target>(const_cast<IRenderVisual*>(static_cast<const IRenderVisual*>(source))->dcast_ParticleCustom()) : nullptr;
    }
    static inline Target cast(Source& source)
    {
        return static_cast<Target>(const_cast<IRenderVisual*>(static_cast<const IRenderVisual*>(&source))->dcast_ParticleCustom());
    }
};

// Specialization for Target = IKinematicsAnimated* from IKinematics
template <typename Target, typename Source>
struct smart_cast_helper<Target, Source,
    std::enable_if_t<
        (std::is_same_v<Target, IKinematicsAnimated*> || std::is_same_v<Target, const IKinematicsAnimated*>) &&
        std::is_base_of_v<IKinematics, Source> && !std::is_base_of_v<IRenderVisual, Source>
    >>
{
    static inline Target cast(Source* source)
    {
        return source ? static_cast<Target>(const_cast<IKinematics*>(static_cast<const IKinematics*>(source))->dcast_PKinematicsAnimated()) : nullptr;
    }
    static inline Target cast(Source& source)
    {
        return static_cast<Target>(const_cast<IKinematics*>(static_cast<const IKinematics*>(&source))->dcast_PKinematicsAnimated());
    }
};

// Specialization for Target = IKinematics* from IKinematicsAnimated
template <typename Target, typename Source>
struct smart_cast_helper<Target, Source,
    std::enable_if_t<
        (std::is_same_v<Target, IKinematics*> || std::is_same_v<Target, const IKinematics*>) &&
        std::is_base_of_v<IKinematicsAnimated, Source> && !std::is_base_of_v<IRenderVisual, Source>
    >>
{
    static inline Target cast(Source* source)
    {
        return source ? static_cast<Target>(const_cast<IKinematicsAnimated*>(static_cast<const IKinematicsAnimated*>(source))->dcast_PKinematics()) : nullptr;
    }
    static inline Target cast(Source& source)
    {
        return static_cast<Target>(const_cast<IKinematicsAnimated*>(static_cast<const IKinematicsAnimated*>(&source))->dcast_PKinematics());
    }
};

// Specialization for Target = IRenderVisual* from IKinematics
template <typename Target, typename Source>
struct smart_cast_helper<Target, Source,
    std::enable_if_t<
        (std::is_same_v<Target, IRenderVisual*> || std::is_same_v<Target, const IRenderVisual*>) &&
        std::is_base_of_v<IKinematics, Source>
    >>
{
    static inline Target cast(Source* source)
    {
        return source ? static_cast<Target>(const_cast<IKinematics*>(static_cast<const IKinematics*>(source))->dcast_RenderVisual()) : nullptr;
    }
    static inline Target cast(Source& source)
    {
        return static_cast<Target>(const_cast<IKinematics*>(static_cast<const IKinematics*>(&source))->dcast_RenderVisual());
    }
};

// Specialization for Target = IRenderVisual* from IKinematicsAnimated
template <typename Target, typename Source>
struct smart_cast_helper<Target, Source,
    std::enable_if_t<
        (std::is_same_v<Target, IRenderVisual*> || std::is_same_v<Target, const IRenderVisual*>) &&
        std::is_base_of_v<IKinematicsAnimated, Source> && !std::is_base_of_v<IKinematics, Source>
    >>
{
    static inline Target cast(Source* source)
    {
        return source ? static_cast<Target>(const_cast<IKinematicsAnimated*>(static_cast<const IKinematicsAnimated*>(source))->dcast_RenderVisual()) : nullptr;
    }
    static inline Target cast(Source& source)
    {
        return static_cast<Target>(const_cast<IKinematicsAnimated*>(static_cast<const IKinematicsAnimated*>(&source))->dcast_RenderVisual());
    }
};

template <typename Target, typename Source>
inline Target smart_cast(Source* source)
{
    return smart_cast_helper<Target, Source>::cast(source);
}

template <typename Target, typename Source>
inline Target smart_cast(Source& source)
{
    return smart_cast_helper<Target, Source>::cast(source);
}

template <typename Target>
inline Target smart_cast(std::nullptr_t)
{
    return nullptr;
}
#endif // SMART_CAST_HELPER_DEFINED
#else
#define TL_FAST_COMPILATION
#include <loki/Typelist.h>

#define PURE_DYNAMIC_CAST_COMPATIBILITY_CHECK

#ifdef DEBUG
//#		define SMART_CAST_STATS_ALL
#endif

#ifndef DECLARE_SPECIALIZATION
#include "smart_cast_impl0.h"
#else
#include "smart_cast_impl2.h"
#define DO_NOT_DECLARE_TYPE_LIST
#endif

#ifdef XRGAME_EXPORTS

DECLARE_SPECIALIZATION(IKinematics, IRenderVisual, dcast_PKinematics);
#undef cast_type_list
#define cast_type_list save_cast_list(IKinematics, IRenderVisual)

DECLARE_SPECIALIZATION(IKinematicsAnimated, IRenderVisual, dcast_PKinematicsAnimated);
#undef cast_type_list
#define cast_type_list save_cast_list(IKinematicsAnimated, IRenderVisual)

DECLARE_SPECIALIZATION(IParticleCustom, IRenderVisual, dcast_ParticleCustom);
#undef cast_type_list
#define cast_type_list save_cast_list(IParticleCustom, IRenderVisual)

DECLARE_SPECIALIZATION(IKinematics, IKinematicsAnimated, dcast_PKinematics);
#undef cast_type_list
#define cast_type_list save_cast_list(IKinematics, IKinematicsAnimated)

DECLARE_SPECIALIZATION(IKinematicsAnimated, IKinematics, dcast_PKinematicsAnimated);
#undef cast_type_list
#define cast_type_list save_cast_list(IKinematicsAnimated, IKinematics)

DECLARE_SPECIALIZATION(IRenderVisual, IKinematicsAnimated, dcast_RenderVisual);
#undef cast_type_list
#define cast_type_list save_cast_list(IRenderVisual, IKinematicsAnimated)

DECLARE_SPECIALIZATION(IRenderVisual, IKinematics, dcast_RenderVisual);
#undef cast_type_list
#define cast_type_list save_cast_list(IRenderVisual, IKinematics)

#ifndef DO_NOT_DECLARE_TYPE_LIST
class ENGINE_API ISpatial;
namespace Feel
{
class ENGINE_API Sound;
}
typedef Feel::Sound Feel__Sound;
template <>
extern Feel::Sound* SmartDynamicCast::smart_cast<Feel::Sound, ISpatial>(ISpatial* p);
add_to_cast_list(Feel__Sound, ISpatial);
#undef cast_type_list
#define cast_type_list save_cast_list(Feel__Sound, ISpatial)
#endif

DECLARE_SPECIALIZATION(IRenderable, ISpatial, dcast_Renderable);
#undef cast_type_list
#define cast_type_list save_cast_list(IRenderable, ISpatial)

DECLARE_SPECIALIZATION(IRender_Light, ISpatial, dcast_Light);
#undef cast_type_list
#define cast_type_list save_cast_list(IRender_Light, ISpatial)

DECLARE_SPECIALIZATION(IGameObject, ISpatial, dcast_GameObject);
#undef cast_type_list
#define cast_type_list save_cast_list(IGameObject, ISpatial)

#ifndef DO_NOT_DECLARE_TYPE_LIST
class IGameObject;
class CGameObject;
add_to_cast_list(CGameObject, IGameObject);
#undef cast_type_list
#define cast_type_list save_cast_list(CGameObject, IGameObject)
#endif

DECLARE_SPECIALIZATION(CEntity, CGameObject, cast_entity);
#undef cast_type_list
#define cast_type_list save_cast_list(CEntity, CGameObject)

DECLARE_SPECIALIZATION(CEntityAlive, CGameObject, cast_entity_alive);
#undef cast_type_list
#define cast_type_list save_cast_list(CEntityAlive, CGameObject)

DECLARE_SPECIALIZATION(CInventoryItem, CGameObject, cast_inventory_item);
#undef cast_type_list
#define cast_type_list save_cast_list(CInventoryItem, CGameObject)

DECLARE_SPECIALIZATION(CInventoryOwner, CGameObject, cast_inventory_owner);
#undef cast_type_list
#define cast_type_list save_cast_list(CInventoryOwner, CGameObject)

DECLARE_SPECIALIZATION(CActor, CGameObject, cast_actor);
#undef cast_type_list
#define cast_type_list save_cast_list(CActor, CGameObject)

DECLARE_SPECIALIZATION(CGameObject, CInventoryOwner, cast_game_object);
#undef cast_type_list
#define cast_type_list save_cast_list(CGameObject, CInventoryOwner)

DECLARE_SPECIALIZATION(CWeapon, CInventoryItem, cast_weapon);
#undef cast_type_list
#define cast_type_list save_cast_list(CWeapon, CInventoryItem)

DECLARE_SPECIALIZATION(CWeapon, CGameObject, cast_weapon);
#undef cast_type_list
#define cast_type_list save_cast_list(CWeapon, CGameObject)

DECLARE_SPECIALIZATION(CFoodItem, CInventoryItem, cast_food_item);
#undef cast_type_list
#define cast_type_list save_cast_list(CFoodItem, CInventoryItem)

DECLARE_SPECIALIZATION(CMissile, CInventoryItem, cast_missile);
#undef cast_type_list
#define cast_type_list save_cast_list(CMissile, CInventoryItem)

DECLARE_SPECIALIZATION(CCustomZone, CGameObject, cast_custom_zone);
#undef cast_type_list
#define cast_type_list save_cast_list(CCustomZone, CGameObject)

DECLARE_SPECIALIZATION(CWeaponMagazined, CWeapon, cast_weapon_magazined);
#undef cast_type_list
#define cast_type_list save_cast_list(CWeaponMagazined, CWeapon)

DECLARE_SPECIALIZATION(CHudItem, CInventoryItem, cast_hud_item);
#undef cast_type_list
#define cast_type_list save_cast_list(CHudItem, CInventoryItem)

DECLARE_SPECIALIZATION(CPhysicsShellHolder, CGameObject, cast_physics_shell_holder);
#undef cast_type_list
#define cast_type_list save_cast_list(CPhysicsShellHolder, CGameObject)

DECLARE_SPECIALIZATION(IInputReceiver, CGameObject, cast_input_receiver);
#undef cast_type_list
#define cast_type_list save_cast_list(IInputReceiver, CGameObject)

DECLARE_SPECIALIZATION(CWeaponAmmo, CInventoryItem, cast_weapon_ammo);
#undef cast_type_list
#define cast_type_list save_cast_list(CWeaponAmmo, CInventoryItem)
/*
        DECLARE_SPECIALIZATION	(CCameraShotEffector, CCameraEffector,		cast_effector_shot);
#		undef cast_type_list
#		define cast_type_list save_cast_list	(CCameraShotEffector,		CCameraEffector)

        DECLARE_SPECIALIZATION	(CEffectorZoomInertion,	CCameraEffector,	cast_effector_zoom_inertion);
#		undef cast_type_list
#		define cast_type_list save_cast_list	(CEffectorZoomInertion,	CCameraEffector)
*/
DECLARE_SPECIALIZATION(CParticlesPlayer, CGameObject, cast_particles_player);
#undef cast_type_list
#define cast_type_list save_cast_list(CParticlesPlayer, CGameObject)

DECLARE_SPECIALIZATION(CArtefact, CGameObject, cast_artefact);
#undef cast_type_list
#define cast_type_list save_cast_list(CArtefact, CGameObject)

DECLARE_SPECIALIZATION(CCustomMonster, CGameObject, cast_custom_monster);
#undef cast_type_list
#define cast_type_list save_cast_list(CCustomMonster, CGameObject)

DECLARE_SPECIALIZATION(CAI_Stalker, CGameObject, cast_stalker);
#undef cast_type_list
#define cast_type_list save_cast_list(CAI_Stalker, CGameObject)

DECLARE_SPECIALIZATION(CScriptEntity, CGameObject, cast_script_entity);
#undef cast_type_list
#define cast_type_list save_cast_list(CScriptEntity, CGameObject)

DECLARE_SPECIALIZATION(CSpaceRestrictor, CGameObject, cast_restrictor);
#undef cast_type_list
#define cast_type_list save_cast_list(CSpaceRestrictor, CGameObject)

DECLARE_SPECIALIZATION(CExplosive, CGameObject, cast_explosive);
#undef cast_type_list
#define cast_type_list save_cast_list(CExplosive, CGameObject)

DECLARE_SPECIALIZATION(CGameObject, CAttachmentOwner, cast_game_object);
#undef cast_type_list
#define cast_type_list save_cast_list(CGameObject, CAttachmentOwner)

DECLARE_SPECIALIZATION(CGameObject, CInventoryItem, cast_game_object);
#undef cast_type_list
#define cast_type_list save_cast_list(CGameObject, CInventoryItem)

DECLARE_SPECIALIZATION(CAttachableItem, CGameObject, cast_attachable_item);
#undef cast_type_list
#define cast_type_list save_cast_list(CAttachableItem, CGameObject)

DECLARE_SPECIALIZATION(CHolderCustom, CGameObject, cast_holder_custom);
#undef cast_type_list
#define cast_type_list save_cast_list(CHolderCustom, CGameObject)

DECLARE_SPECIALIZATION(CAttachmentOwner, CGameObject, cast_attachment_owner);
#undef cast_type_list
#define cast_type_list save_cast_list(CAttachmentOwner, CGameObject)

DECLARE_SPECIALIZATION(CEatableItem, CInventoryItem, cast_eatable_item);
#undef cast_type_list
#define cast_type_list save_cast_list(CEatableItem, CInventoryItem)

DECLARE_SPECIALIZATION(CBaseMonster, CGameObject, cast_base_monster);
#undef cast_type_list
#define cast_type_list save_cast_list(CBaseMonster, CGameObject)

#endif

DECLARE_SPECIALIZATION(CSE_Abstract, CSE_ALifeInventoryItem, cast_abstract);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_Abstract, CSE_ALifeInventoryItem)

DECLARE_SPECIALIZATION(CSE_Abstract, CSE_ALifeTraderAbstract, cast_abstract);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_Abstract, CSE_ALifeTraderAbstract)

DECLARE_SPECIALIZATION(CSE_Abstract, CSE_ALifeGroupAbstract, cast_abstract);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_Abstract, CSE_ALifeGroupAbstract)

DECLARE_SPECIALIZATION(CSE_Abstract, CSE_ALifeSchedulable, cast_abstract);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_Abstract, CSE_ALifeSchedulable)

DECLARE_SPECIALIZATION(CSE_ALifeGroupAbstract, CSE_Abstract, cast_group_abstract);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeGroupAbstract, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_ALifeSchedulable, CSE_Abstract, cast_schedulable);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeSchedulable, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_ALifeInventoryItem, CSE_Abstract, cast_inventory_item);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeInventoryItem, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_ALifeTraderAbstract, CSE_Abstract, cast_trader_abstract);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeTraderAbstract, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_Visual, CSE_Abstract, visual);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_Visual, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_Motion, CSE_Abstract, motion);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_Motion, CSE_Abstract)

DECLARE_SPECIALIZATION(IServerEntityShape, CSE_Abstract, shape);
#undef cast_type_list
#define cast_type_list save_cast_list(IServerEntityShape, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_Abstract, CSE_PHSkeleton, cast_abstract);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_Abstract, CSE_PHSkeleton)

DECLARE_SPECIALIZATION(CSE_ALifeObject, CSE_Abstract, cast_alife_object);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeObject, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_ALifeDynamicObject, CSE_Abstract, cast_alife_dynamic_object);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeDynamicObject, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_ALifeItemAmmo, CSE_Abstract, cast_item_ammo);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeItemAmmo, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_ALifeItemWeapon, CSE_Abstract, cast_item_weapon);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeItemWeapon, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_ALifeItemDetector, CSE_Abstract, cast_item_detector);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeItemDetector, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_ALifeMonsterAbstract, CSE_Abstract, cast_monster_abstract);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeMonsterAbstract, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_ALifeHumanAbstract, CSE_Abstract, cast_human_abstract);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeHumanAbstract, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_ALifeAnomalousZone, CSE_Abstract, cast_anomalous_zone);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeAnomalousZone, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_ALifeTrader, CSE_Abstract, cast_trader);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeTrader, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_ALifeCreatureAbstract, CSE_Abstract, cast_creature_abstract);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeCreatureAbstract, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_ALifeSmartZone, CSE_Abstract, cast_smart_zone);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeSmartZone, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_ALifeOnlineOfflineGroup, CSE_Abstract, cast_online_offline_group);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeOnlineOfflineGroup, CSE_Abstract)

DECLARE_SPECIALIZATION(CSE_ALifeItemPDA, CSE_Abstract, cast_item_pda);
#undef cast_type_list
#define cast_type_list save_cast_list(CSE_ALifeItemPDA, CSE_Abstract)

#ifndef DO_NOT_DECLARE_TYPE_LIST
#include "smart_cast_impl1.h"
#endif
#endif // PURE_DYNAMIC_CAST

#endif // SMART_CAST_H
