#pragma once

namespace aurageek::services {
// Store is Preferences on device, an isolated in-memory store in host tests.
// Never erase a namespace: cloud identity and stock request protection survive.
template<class Store> class FactoryReset {
 public:
  enum class Result { None, Completed, Failed };
  static bool request() { return setFlag("aura-web", "reset-pending"); }
  static bool userNetworkOnly() {
    Store p;if(!p.begin("aura-net",true))return false;
    const bool value=p.getBool("user-only",false);p.end();return value;
  }
  static Result resume() {
    Store p;if(!p.begin("aura-web",false))return Result::Failed;
    const bool pending=p.getBool("reset-pending",false);p.end();
    if(!pending)return Result::None;
    // Persist policy BEFORE deleting the user network; no developer fallback.
    if(!setFlag("aura-net","user-only") ||
       !erase("aura-net","network") || !erase("aura-web","config") ||
       !erase("aura-web","custom-id") || !erase("aura-ui","settings") ||
       !setFlag("aura-web","setup") || !erase("aura-web","reset-pending"))
      return Result::Failed;
    return Result::Completed;
  }
 private:
  static bool setFlag(const char* space,const char* key) {
    Store p;if(!p.begin(space,false))return false;
    const bool ok=p.putBool(key,true)==1&&p.getBool(key,false);p.end();return ok;
  }
  static bool erase(const char* space,const char* key) {
    Store p;if(!p.begin(space,false))return false;
    const bool ok=(!p.isKey(key)||p.remove(key))&&!p.isKey(key);p.end();return ok;
  }
};
}
