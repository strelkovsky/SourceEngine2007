// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: 
//


#ifndef TE_SHOTGUN_SHOT_H
#define TE_SHOTGUN_SHOT_H
#ifdef _WIN32
#pragma once
#endif


void TE_FireBullets( 
	int	iPlayerIndex,
	const Vector &vOrigin,
	const QAngle &vAngles,
	int	iWeaponID,
	int	iMode,
	int iSeed,
	float flSpread 
	);

void TE_PlantBomb( int iPlayerIndex, const Vector &vOrigin );


#endif // TE_SHOTGUN_SHOT_H
