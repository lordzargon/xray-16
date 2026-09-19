////////////////////////////////////////////////////////////////////////////
// Module : XR_IOConsole_get.cpp
// Created : 17.05.2008
// Author : Evgeniy Sokolov
// Description : Console`s get-functions class implementation
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "XR_IOConsole.h"
#include "xr_ioc_cmd.h"

bool CConsole::GetBool(pcstr cmd) const
{
    IConsole_Command* cc = GetCommand(cmd);
    return cc ? cc->GetBool() : false;
}

float CConsole::GetFloat(pcstr cmd, float& min, float& max) const
{
    min = 0.0f;
    max = 0.0f;
    IConsole_Command* cc = GetCommand(cmd);
    return cc ? cc->GetFloat(min, max) : 0.0f;
}

IConsole_Command* CConsole::GetCommand(pcstr cmd) const
{
    const auto it = Commands.find(cmd);
    if (it == Commands.end())
        return NULL;
    else
        return it->second;
}

int CConsole::GetInteger(pcstr cmd, int& min, int& max) const
{
    min = 0;
    max = 1;
    IConsole_Command* cc = GetCommand(cmd);
    return cc ? cc->GetInteger(min, max) : 0;
}

pcstr CConsole::GetString(pcstr cmd) const
{
    IConsole_Command* cc = GetCommand(cmd);
    if (!cc)
        return NULL;

    static IConsole_Command::TStatus stat;
    cc->GetStatus(stat);
    return stat;
}

pcstr CConsole::GetToken(pcstr cmd) const { return GetString(cmd); }
const xr_token* CConsole::GetXRToken(pcstr cmd) const
{
    IConsole_Command* cc = GetCommand(cmd);
    return cc ? cc->GetToken() : nullptr;
}

Fvector* CConsole::GetFVectorPtr(pcstr cmd) const
{
    IConsole_Command* cc = GetCommand(cmd);
    return cc ? cc->GetFVectorPtr() : nullptr;
}

Fvector CConsole::GetFVector(pcstr cmd) const
{
    Fvector* pV = GetFVectorPtr(cmd);
    if (pV)
    {
        return *pV;
    }
    return Fvector().set(0.0f, 0.0f, 0.0f);
}

