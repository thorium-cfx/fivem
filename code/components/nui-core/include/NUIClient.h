/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#pragma once

#include <mutex>

#include <NUIWindow.h>
#include <include/cef_client.h>

#include <CefOverlay.h>

#include <regex>

class NUIClient : public CefClient, public CefLifeSpanHandler, public CefDisplayHandler, public CefContextMenuHandler, public CefLoadHandler, public CefRequestHandler, public CefAudioHandler
{
private:
	NUIWindow* m_window;
	bool m_windowValid;

	std::recursive_mutex m_windowLock;

	CefRefPtr<CefBrowser> m_browser;

	std::vector<std::regex> m_requestBlacklist;

public:
	NUIClient(NUIWindow* window);

	inline NUIWindow*	GetWindow()					{ return m_window;		}
	inline CefBrowser*	GetBrowser()				{ return m_browser.get();		}

	inline bool			GetWindowValid()			{ return m_windowValid;			}
	inline void			SetWindowValid(bool valid)	{ m_windowValid = valid;		}

	inline std::recursive_mutex& GetWindowLock()
	{
		return m_windowLock;
	}

public:
	static fwEvent<NUIClient*> OnClientCreated;

// CefClient
protected:
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
	virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;
	virtual CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override;
	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override;
	virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override;
	virtual CefRefPtr<CefAudioHandler> GetAudioHandler() override;
	virtual CefRefPtr<CefRequestHandler> GetRequestHandler() override;

	virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override;

public:
	typedef std::function<bool(CefRefPtr<CefBrowser>, CefRefPtr<CefProcessMessage>)> TProcessMessageHandler;

	void AddProcessMessageHandler(std::string key, TProcessMessageHandler handler);

private:
	std::map<std::string, TProcessMessageHandler> m_processMessageHandlers;

	CefRefPtr<CefRenderHandler> m_renderHandler;

// CefLifeSpanHandler
protected:
	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;

	virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

// CefLoadHandler
protected:
	virtual void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, TransitionType transitionType) override;

	virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) override;

// CefContextMenuHandler
protected:
	virtual void OnBeforeContextMenu(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefContextMenuParams> params, CefRefPtr<CefMenuModel> model) override;

// CefDisplayHandler
protected:
	virtual bool OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level, const CefString& message, const CefString& source, int line) override;

// CefAudioHandler
protected:
	virtual void OnAudioStreamStarted(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		int audio_stream_id,
		int channels,
		ChannelLayout channel_layout,
		int sample_rate,
		int frames_per_buffer) override;

	virtual void OnAudioStreamPacket(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		int audio_stream_id,
		const float** data,
		int frames,
		int64 pts) override;

	virtual void OnAudioStreamStopped(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		int audio_stream_id) override;

public:
	virtual void OnAudioCategoryConfigure(const std::string& frame, const std::string& category);

private:
	std::map<std::pair<int, int>, std::tuple<std::shared_ptr<nui::IAudioStream>, nui::AudioStreamParams>> m_audioStreams;

	std::multimap<std::string, std::pair<int, int>> m_audioStreamsByFrame;

	std::map<std::string, std::string> m_audioFrameCategories;

// CefRequestHandler
protected:
	virtual ReturnValue OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request, CefRefPtr<CefRequestCallback> callback) override;

	virtual void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser, TerminationStatus status) override;

	IMPLEMENT_REFCOUNTING(NUIClient);
};
