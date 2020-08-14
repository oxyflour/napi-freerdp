#include <napi.h>
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <freerdp/freerdp.h>
#include <freerdp/cache/cache.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/channels/channels.h>
#include <freerdp/client/cmdline.h>

#include <map>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

struct Connection;
std::map<freerdp *, Connection *> conns;

typedef struct Connection {
  freerdp *rdp;
  bool started;
  Napi::ThreadSafeFunction func;

  template <typename Callback> auto emit(const char *evt, Callback cb) {
    func.BlockingCall([evt, cb](Napi::Env env, Napi::Function func) {
      func.Call({ Napi::String::New(env, evt), cb(env) });
    });
  }
  auto emit(const char *evt, std::string msg) {
    emit(evt, [msg](Napi::Env env) { return Napi::Error::New(env, msg).Value(); });
  }
  auto emit(const char *evt, const char *msg = "") {
    emit(evt, std::string(msg));
  }

  BOOL onPreConnect() {
    auto settings = rdp->settings;
    settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
    settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = TRUE;
    settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
    settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = TRUE;
    settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
    settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
    settings->OrderSupport[NEG_MEMBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_MEM3BLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = TRUE;
    settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
    settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
    settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
    settings->OrderSupport[NEG_POLYGON_SC_INDEX] = TRUE;
    settings->OrderSupport[NEG_POLYGON_CB_INDEX] = TRUE;
    settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = TRUE;
    settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = TRUE;
    freerdp_channels_pre_connect(rdp->context->channels, rdp);
    return TRUE;
  }
  BOOL onPostConnect() {
    rdp->update->BeginPaint    = [](rdpContext *context) { return conns[context->instance]->onBeginPaint(); };
    rdp->update->EndPaint      = [](rdpContext *context) { return conns[context->instance]->onEndPaint(); };
    rdp->update->DesktopResize = [](rdpContext *context) { return conns[context->instance]->onResizeDesktop(); };
    rdp->context->cache = cache_new(rdp->settings);
    gdi_init(rdp, CLRCONV_ALPHA | CLRBUF_32BPP, NULL);
    freerdp_channels_post_connect(rdp->context->channels, rdp);
    return TRUE;
  }

  void onBeginPaint() {
    auto hwnd = rdp->context->gdi->primary->hdc->hwnd;
    hwnd->invalid->null = 1;
    hwnd->ninvalid = 0;
    emit("begin-paint");
  }
  void onEndPaint() {
    // https://github.com/bloomapi/node-freerdp/blob/master/rdp.cc
    auto gdi = rdp->context->gdi;
    auto rect = gdi->primary->hdc->hwnd->invalid;
    auto x = rect->x, y = rect->y, w = rect->w, h = rect->h;
    auto bpp = gdi->bytesPerPixel;
    auto bytes = w * h * bpp;
    auto buffer = new BYTE[bytes];
    for (int i = y, dst_pos = 0; i < y + h; i ++, dst_pos += w * bpp) {
      auto start_pos = (i * gdi->width * bpp) + (x * bpp);
      auto src = gdi->primary_buffer + start_pos;
      auto dst = buffer + dst_pos;
      memcpy(dst, src, w * bpp);
    }
    emit("paint", [x, y, w, h, buffer, bytes](Napi::Env env) {
      auto ret = Napi::Object::New(env);
      ret.Set("x", Napi::Number::New(env, x));
      ret.Set("y", Napi::Number::New(env, y));
      ret.Set("w", Napi::Number::New(env, w));
      ret.Set("h", Napi::Number::New(env, h));
      ret.Set("d", Napi::ArrayBuffer::New(env, buffer, bytes,
        [](Napi::Env env, void *data, BYTE *buffer) { delete buffer; }, buffer));
      return ret;
    });
  }
  void onResizeDesktop() {
    emit("resize");
  }

  int onContextNew(rdpContext *context) {
    context->channels = freerdp_channels_new();
    return 0;
  }
  void onContextFree(rdpContext *context) {
  }

  void prepare(Napi::Object options) {
    rdp->PreConnect  = [](freerdp *rdp) { return conns[rdp]->onPreConnect(); };
    rdp->PostConnect = [](freerdp *rdp) { return conns[rdp]->onPostConnect(); };
    rdp->ContextNew  = [](freerdp *rdp, rdpContext *context) { return conns[rdp]->onContextNew(context); };
    rdp->ContextFree = [](freerdp *rdp, rdpContext *context) { return conns[rdp]->onContextFree(context); };
    freerdp_context_new(rdp);

    auto settings = rdp->settings;
    // TODO: add more options
    if (options.Has("serverHostName")) {
      settings->ServerHostname = strdup(std::string(options.Get("serverHostName").ToString()).c_str());
    }
    if (options.Has("username")) {
      settings->Username = strdup(std::string(options.Get("username").ToString()).c_str());
    }
    if (options.Has("password")) {
      settings->Password = strdup(std::string(options.Get("password").ToString()).c_str());
    }
    if (options.Has("ignoreCertificate")) {
      settings->IgnoreCertificate = options.Get("ignoreCertificate").ToBoolean().Value();
    }
    if (options.Has("desktopWidth")) {
      settings->DesktopWidth = options.Get("desktopWidth").ToNumber().Int64Value();
    }
    if (options.Has("desktopHeight")) {
      settings->DesktopHeight = options.Get("desktopHeight").ToNumber().Int64Value();
    }
    settings->BitmapCacheEnabled = true;
    // FIXME: segmentation fault without the following settings
    // https://github.com/FreeRDP/FreeRDP/blob/780d451afad21a22d2af6bd030ee71311856f038/client/common/cmdline.c#L1451
    settings->NlaSecurity = false;
    settings->TlsSecurity = false;

    freerdp_client_load_addins(rdp->context->channels, rdp->settings);
  }

  void run() {
    conns[rdp] = this;
    started = true;

    if (!freerdp_connect(rdp)) {
      auto code = freerdp_error_info(rdp);
      emit("error", std::string("code ") + std::to_string(code));
      return;
    }

    emit("connected");

    // https://github.com/FreeRDP/FreeRDP/blob/780d451afad21a22d2af6bd030ee71311856f038/client/Sample/freerdp.c
    void *rfds[32] = { 0 }, *wfds[32] = { 0 };
    fd_set rfdn, wfdn;
    auto channels = rdp->context->channels;
    while (started) {
      int rn = 0, wn = 0;
      if (freerdp_get_fds(rdp, rfds, &rn, wfds, &wn) != TRUE) {
        emit("error", "get fds failed");
        break;
      }
      if (freerdp_channels_get_fds(channels, rdp, rfds, &rn, wfds, &wn) != TRUE) {
        emit("error", "get channel fds failed");
        break;
      }

      auto max_fds = 0;
      FD_ZERO(&rfdn);
      FD_ZERO(&wfdn);
      for (auto i = 0; i < rn; i ++) {
        auto fds = (int) (long) (rfds[i]);
        if (fds > max_fds) {
          max_fds = fds;
        }
        FD_SET(fds, &rfdn);
      }
      if (max_fds == 0) {
        break;
      }

      if (select(max_fds + 1, &rfdn, &wfdn, NULL, NULL) == -1) {
        if (!(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS || errno == EINTR)) {
          emit("error", "select error: " + std::to_string(errno));
          break;
        }
      }
      if (freerdp_check_fds(rdp) != TRUE) {
        emit("error", "failed to check fd: " + std::to_string(freerdp_error_info(rdp)));
        break;
      }
      if (freerdp_channels_check_fds(channels, rdp) != TRUE) {
        emit("error", "failed to check fd for channels: " + std::to_string(freerdp_error_info(rdp)));
        break;
      }
    }

    freerdp_channels_close(channels, rdp);
    freerdp_channels_free(channels);
    gdi_free(rdp);
    cache_free(rdp->context->cache);
    freerdp_free(rdp);

    emit("disconnected");

    started = false;
    func.Release();
    conns.erase(rdp);
  }
} Connection;

Napi::Value Connect(const Napi::CallbackInfo& info) {
  auto conn = new Connection();
  conn->rdp = freerdp_new();
  conn->func = Napi::ThreadSafeFunction::New(info.Env(), info[1].As<Napi::Function>(), "", 0, 1);
  conn->prepare(info[0].ToObject());
  std::thread([conn]{ conn->run(); delete conn; }).detach();
  return Napi::Number::New(info.Env(), reinterpret_cast<int64_t>(conn->rdp));
}

Napi::Value Disconnect(const Napi::CallbackInfo& info) {
  auto rdp = reinterpret_cast<freerdp *>(info[0].ToNumber().Int64Value());
  if (conns.count(rdp)) {
    conns[rdp]->started = false;
  } else {
    Napi::Error::New(info.Env(), "no such connection").ThrowAsJavaScriptException();
  }
  return info.Env().Undefined();
}

Napi::Value SendKey(const Napi::CallbackInfo& info) {
  auto rdp = reinterpret_cast<freerdp *>(info[0].ToNumber().Int64Value());
  if (conns.count(rdp)) {
    freerdp_input_send_keyboard_event_ex(rdp->input,
      info[2].ToBoolean().Value(), info[1].ToNumber().Int32Value());
  } else {
    Napi::Error::New(info.Env(), "no such connection").ThrowAsJavaScriptException();
  }
  return info.Env().Undefined();
}

Napi::Value SendMouse(const Napi::CallbackInfo& info) {
  auto rdp = reinterpret_cast<freerdp *>(info[0].ToNumber().Int64Value());
  if (conns.count(rdp)) {
    UINT16 flags = 0;
    auto options = info[3].ToObject();
    if (options.Has("left")) {
      flags |= PTR_FLAGS_BUTTON1;
      if (options.Get("left").ToBoolean().Value()) {
        flags |= PTR_FLAGS_DOWN;
      }
    }
    if (options.Has("right")) {
      flags |= PTR_FLAGS_BUTTON2;
      if (options.Get("right").ToBoolean().Value()) {
        flags |= PTR_FLAGS_DOWN;
      }
    }
    if (options.Has("middle")) {
      flags |= PTR_FLAGS_BUTTON3;
      if (options.Get("middle").ToBoolean().Value()) {
        flags |= PTR_FLAGS_DOWN;
      }
    }
    if (flags == 0) {
      flags |= PTR_FLAGS_MOVE;
    }
    freerdp_input_send_mouse_event(rdp->input, flags,
      info[1].ToNumber().Int64Value(), info[2].ToNumber().Int64Value());
  } else {
    Napi::Error::New(info.Env(), "no such connection").ThrowAsJavaScriptException();
  }
  return info.Env().Undefined();
}

// https://stackoverflow.com/questions/77005/how-to-automatically-generate-a-stacktrace-when-my-program-crashes
void HandleSegmentFault(int sig) {
  void *array[50];
  auto size = backtrace(array, 50);
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  signal(SIGSEGV, HandleSegmentFault);
  freerdp_channels_global_init();
  exports.Set(Napi::String::New(env, "connect"),    Napi::Function::New(env, Connect));
  exports.Set(Napi::String::New(env, "disconnect"), Napi::Function::New(env, Disconnect));
  exports.Set(Napi::String::New(env, "sendKey"),    Napi::Function::New(env, SendKey));
  exports.Set(Napi::String::New(env, "sendMouse"),  Napi::Function::New(env, SendMouse));
  return exports;
}

NODE_API_MODULE(freerdp, Init)
