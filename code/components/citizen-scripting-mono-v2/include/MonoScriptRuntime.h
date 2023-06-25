/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#pragma once

#include "StdInc.h"

#include <deque>

#include <fxScripting.h>
#include <ExternalFunctions/IScriptRuntimeExternalFunctions.h>

#include <Resource.h>
#include <ManifestVersion.h>

#include <om/OMComponent.h>

#include "MonoMethods.h"

namespace fx::mono
{
class MonoScriptRuntime : public fx::OMClass<MonoScriptRuntime, IScriptRuntime, IScriptFileHandlingRuntime, IScriptTickRuntimeWithBookmarks,
	IScriptEventRuntime, IScriptRefRuntime, IScriptMemInfoRuntime, /*IScriptStackWalkingRuntime,*/ IScriptRuntimeExternalFunctions, IScriptDebugRuntime, IScriptProfiler>
{
private:
	int m_instanceId;
	std::string m_name;
	std::string m_resourceName;
	MonoDomain* m_appDomain;
	int32_t m_appDomainId;

	// Direct host access
	IScriptHost* m_scriptHost;
	IScriptHostWithResourceData* m_resourceHost;
	IScriptHostWithManifest* m_manifestHost;
	IScriptHostWithBookmarks* m_bookmarkHost;

	fx::OMPtr<IScriptRuntimeHandler> m_handler;
	fx::Resource* m_parentObject;
	IDebugEventListener* m_debugListener;
	std::unordered_map<std::string, int> m_scriptIds;
	uint64_t m_scheduledTime = ~uint64_t(0);

	// method targets
	Method m_loadAssembly;

	// method thunks, these are for calls that require performance
	Thunk<uint32_t(uint64_t gameTime, bool profiling)> m_tick;
	Thunk<uint32_t(MonoString* eventName, const char* argsSerialized, uint32_t serializedSize, MonoString* sourceId, uint64_t gameTime, bool profiling)> m_triggerEvent;

	Thunk<uint32_t(int32_t refIndex, char* argsSerialized, uint32_t argsSize, char** retvalSerialized, uint32_t* retvalSize, uint64_t gameTime, bool profiling)> m_callRef;
	Thunk<void(int32_t refIndex, int32_t* newRefIdx)> m_duplicateRef = nullptr;
	Thunk<void(int32_t refIndex)> m_removeRef = nullptr;

	Thunk<uint64_t(size_t privateId, const char* arguments, size_t argumentsSize, uint8_t& status, const char*& result, size_t& resultSize, size_t asyncResultId, uint64_t gameTime, bool profiling)> m_invokeExport = nullptr;
	Thunk<uint64_t(size_t asyncResultId, uint8_t status, const char* arguments, size_t argumentsSize, uint64_t gameTime, bool profiling)> m_asyncResult = nullptr;

	Thunk<int64_t()> m_getMemoryUsage = nullptr;
	Thunk<void()> m_startProfiling = nullptr;
	Thunk<void()> m_stopProfiling = nullptr;

public:
	MonoScriptRuntime();

	virtual ~MonoScriptRuntime() override = default;

	MonoArray* CanonicalizeRef(int referenceId) const;

	bool ReadAssembly(MonoString* name, MonoArray** assembly, MonoArray** symbols) const;

private:
	void InitializeMethods(MonoImage* monoImage);

public:
	NS_DECL_ISCRIPTRUNTIME;

	NS_DECL_ISCRIPTFILEHANDLINGRUNTIME;

	NS_DECL_ISCRIPTTICKRUNTIMEWITHBOOKMARKS;

	void ScheduleTick(uint64_t timeMilliseconds);

	NS_DECL_ISCRIPTEVENTRUNTIME;

	NS_DECL_ISCRIPTREFRUNTIME;

	NS_DECL_ISCRIPTMEMINFORUNTIME;

	// NS_DECL_ISCRIPTSTACKWALKINGRUNTIME;
	 
	NS_DECL_ISCRIPTRUNTIME_EXTERNALFUNCTIONS;

	void RegisterExport(MonoString* exportName, size_t privateId, size_t binding);
	uint8_t InvokeExternalExport(MonoString* resourceName, MonoString* exportName, MonoArray* arguments, const char*& resultData, size_t& resultSize, uint64_t& asyncResultId);

	NS_DECL_ISCRIPTDEBUGRUNTIME;

	NS_DECL_ISCRIPTPROFILER;
};

inline MonoScriptRuntime::MonoScriptRuntime()
{
	m_instanceId = rand();
	m_name = "ScriptDomain_" + std::to_string(m_instanceId);
}

inline void MonoScriptRuntime::ScheduleTick(uint64_t timeMilliseconds)
{
	if (timeMilliseconds < m_scheduledTime)
	{
		m_scheduledTime = timeMilliseconds;
		m_bookmarkHost->ScheduleBookmark(this, 0, timeMilliseconds * 1000); // positive values are expected to me microseconds and absolute
	}
}
}
