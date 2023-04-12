#define public_key_legacy_prefix public_key_legacy_prefix_eos
#include <fc/crypto/public_key.hpp>
#undef public_key_legacy_prefix
#include "config.hpp"

#include "v8/src/flags/flags.h"
#include "fibjs.h"
#include "object.h"
#include "Fiber.h"
#include "console.h"
#include "SandBox.h"
#include "EventEmitter.h"

#include <appbase/application.hpp>
#include <appbase/plugin.hpp>

#include <eosio/chain_api_plugin/chain_api_plugin.hpp>
#include <eosio/chain_plugin/chain_plugin.hpp>
#include <eosio/db_size_api_plugin/db_size_api_plugin.hpp>
#include <eosio/http_client_plugin/http_client_plugin.hpp>
#include <eosio/http_plugin/http_plugin.hpp>
#include <eosio/net_api_plugin/net_api_plugin.hpp>
#include <eosio/net_plugin/net_plugin.hpp>
#include <eosio/producer_api_plugin/producer_api_plugin.hpp>
#include <eosio/producer_plugin/producer_plugin.hpp>
#include <eosio/resource_monitor_plugin/resource_monitor_plugin.hpp>
#include <eosio/trace_api/trace_api_plugin.hpp>
#include <eosio/txn_test_gen_plugin/txn_test_gen_plugin.hpp>
#include <eosio/wallet_api_plugin/wallet_api_plugin.hpp>
#include <eosio/wallet_plugin/wallet_plugin.hpp>
// #include <eosio/version/version.hpp>

#include <fc/log/logger_config.hpp>
#include <fc/log/appender.hpp>
#include <fc/exception/exception.hpp>

#include "ifs/chain.h"
#include "emitter_plugin.h"
#include "gitinfo.h"

// only for config.hpp
using namespace std;
#include <nodeos/config.hpp>

using namespace appbase;
using namespace eosio;

namespace fc {
namespace crypto {
    namespace config {
        // public key prefix
        const char* public_key_legacy_prefix = "DM";
    };
};
};

namespace eosio {
namespace chain {
    static const char* base_root_key = "6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV";
    std::string dmc_root_key = "DM6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV";
}
} // namespace eosio::chain

namespace appbase {
const char* appbase_version_string = "v" GIT_INFO;
} // namespace appbase

namespace fibjs {

extern obj_ptr<NObject> g_info;
extern exlib::string appname;

DECLARE_MODULE(chain);

enum return_codes {
    OTHER_FAIL = -2,
    INITIALIZE_FAIL = -1,
    SUCCESS = 0,
    BAD_ALLOC = 1,
    DATABASE_DIRTY = 2,
    FIXED_REVERSIBLE = 3,
    EXTRACTED_GENESIS = 4
};

static std::vector<exlib::string> s_args;
static std::vector<exlib::string> bool_switch_args = {
    /* chain_plugin.cpp */
    "contracts-console",
    "deep-mind",
    "disable-ram-billing-notify-checks",
    "disable-all-subjective-mitigations",
    "print-genesis-json",
    "print-build-info",
    "force-all-checks",
    "disable-replay-opts",
    "replay-blockchain",
    "hard-replay-blockchain",
    "delete-all-blocks",
    /* http_plugin.cpp */
    "access-control-allow-credentials",
    "verbose-http-errors",
    /* producer_plugin.cpp */
    "enable-stale-production",
    "pause-on-startup",
    "disable-api-persisted-trx",
    /* state_history_plugin.cpp */
    "delete-state-history",
    "trace-history",
    "chain-state-history",
    "trace-history-debug-mode",
    /* trace_api_plugin.cpp */
    /* This option is mutually exclusive with trace-rpc-api */
    "trace-no-abis"
};

static bool s_state_started = false;
static bool s_state_stopped = false;

void init_eos()
{
    appname = "Chain OS V" GIT_INFO ". Based on fibjs";
    g_info->add("chain", appbase_version_string);

    app().set_version(eosio::nodeos::config::version);

    app().register_plugin<chain_api_plugin>();
    app().register_plugin<chain_plugin>();
    app().register_plugin<db_size_api_plugin>();
    app().register_plugin<http_client_plugin>();
    app().register_plugin<http_plugin>();
    app().register_plugin<net_api_plugin>();
    app().register_plugin<net_plugin>();
    app().register_plugin<producer_api_plugin>();
    app().register_plugin<producer_plugin>();

    app().register_plugin<resource_monitor_plugin>();
    app().register_plugin<trace_api_plugin>();
    app().register_plugin<emitter_plugin>();

    auto root = fc::app_path();
    app().set_default_data_dir(root / "eosio/nodeos/data");
    app().set_default_config_dir(root / "eosio/nodeos/config");
    s_args.push_back("chain");
}

static result_t emit_event(std::string event)
{
    Isolate* isolate = Isolate::current();

    JSFiber::EnterJsScope s;
    v8::HandleScope handle_scope(isolate->m_isolate);

    JSTrigger t(isolate->m_isolate, chain_base::class_info().getModule(isolate));
    bool r = false;

    result_t hr = t._emit(event, NULL, 0, r);
    if (hr < 0) {
        elog("error occurs when emit ${event} event", ("event", event));
        app().quit();
    }
    return 0;
}

result_t chain_base::load(exlib::string name, v8::Local<v8::Object> cfg)
{
    exlib::string plug_name = "eosio::" + name + "_plugin";
    auto plugin = app().find_plugin(plug_name.c_str());

    if (!plugin)
        return CHECK_ERROR(Runtime::setError("Plugin " + name + " not found."));

    Isolate* isolate = Isolate::current();
    v8::Local<v8::Context> context = isolate->context();

    if (plugin->get_state() == abstract_plugin::registered) {
        options_description cli, opt;

        plugin->set_program_options(cli, opt);

        s_args.push_back("--plugin");
        s_args.push_back(plug_name);

        JSArray ks = cfg->GetPropertyNames(context);

        int32_t len = ks->Length();
        int32_t i;
        result_t hr;

        for (i = 0; i < len; i++) {
            const char* argv[3] = { "dmc" };

            JSValue k = ks->Get(context, i);
            exlib::string key(isolate->toString(k));

            argv[1] = key.c_str();

            if (opt.find_nothrow(argv[1], true) == NULL && cli.find_nothrow(argv[1], true) == NULL)
                return CHECK_ERROR(Runtime::setError("unrecognised option: " + key));

            bool isMultiOpt = false;
            options_description _opt = (opt.find_nothrow(argv[1], true) == NULL) ? cli : opt;

            if (_opt.find_nothrow(argv[1], true)->semantic()->max_tokens() > 0) {
                JSValue kss = cfg->Get(context, k);
                if (kss->IsArray())
                    isMultiOpt = true;
            }

            if (!isMultiOpt) {
                s_args.push_back("--" + key);
                JSValue mk = cfg->Get(context, k);

                exlib::string value(isolate->toString(mk));
                if (std::find(bool_switch_args.begin(), bool_switch_args.end(), key) != bool_switch_args.end()) {
                    if (mk->IsFalse())
                        s_args.pop_back();
                    else if (mk->IsTrue())
                        continue;
                } else {
                    s_args.push_back(isolate->toString(mk));
                }

            } else {
                JSValue sk = cfg->Get(context, k);
                const JSArray vals = JSArray::Cast(sk);

                if (vals->Length() == 0)
                    return CHECK_ERROR(Runtime::setError("empty option parameters: " + key));

                for (int32_t j = 0; j < vals->Length(); j++) {
                    s_args.push_back("--" + key);
                    JSValue sj = vals->Get(context, j);
                    s_args.push_back(isolate->toString(sj));
                }
            }
        }
    }

    return 0;
}

result_t chain_base::load(v8::Local<v8::Object> cfgs)
{
    Isolate* isolate = Isolate::current();
    v8::Local<v8::Context> context = isolate->context();
    JSArray ks = cfgs->GetPropertyNames(context);
    int32_t len = ks->Length();
    int32_t i;
    result_t hr;

    for (i = 0; i < len; i++) {
        JSValue k = ks->Get(context, i);

        v8::Local<v8::Object> o;
        JSValue v = cfgs->Get(context, k);
        hr = GetArgumentValue(v, o, true);
        if (hr < 0)
            return hr;

        load(isolate->toString(k), o);
    }

    return 0;
}

result_t chain_base::start()
{
    class _thread : public exlib::OSThread {
    public:
        _thread()
            : m_main(Isolate::current())
        {
            m_main->Ref();
        }

    public:
        virtual void Run()
        {
            try {

                std::vector<char*> s_argv;

                s_argv.resize(s_args.size());
                for (int32_t i = 0; i < (int32_t)s_args.size(); i++)
                    s_argv[i] = s_args[i].c_buffer();

                EOS_ASSERT(app().initialize<>(s_argv.size(), s_argv.data()), eosio::chain::chain_exception, "INITIALIZE_FAIL");
                app().startup();

                syncCall(m_main, emit_event, "load");

                app().exec();
            } catch (const fc::exception& e) {
                elog("${e}", ("e", e.to_detail_string()));
                syncCall(m_main, emit_event, "error");
            } catch (const chain_exception& e) {
                elog("${e}", ("e", e.to_detail_string()));
                syncCall(m_main, emit_event, "error");
            } catch (const boost::interprocess::bad_alloc& e) {
                elog("bad alloc");
                syncCall(m_main, emit_event, "error");
            } catch (const boost::exception& e) {
                elog("${e}", ("e", boost::diagnostic_information(e)));
                syncCall(m_main, emit_event, "error");
            } catch (const std::runtime_error& e) {
                if (std::string(e.what()) == "database dirty flag set") {
                    elog("database dirty flag set (likely due to unclean shutdown): replay required");
                } else if (std::string(e.what()) == "database metadata dirty flag set") {
                    elog("database metadata dirty flag set (likely due to unclean shutdown): replay required");
                } else {
                    elog("${e}", ("e", e.what()));
                }
                syncCall(m_main, emit_event, "error");
            } catch (const std::exception& e) {
                elog("${e}", ("e", e.what()));
                syncCall(m_main, emit_event, "error");
            } catch (...) {
                elog("unknown exception");
                syncCall(m_main, emit_event, "error");
            }
            syncCall(m_main, emit_event, "close");
            m_main->Unref();
        }

        static result_t init_proc(_thread* p)
        {
            p->start();
            return 0;
        }

    private:
        Isolate* m_main;
    };

    if (s_state_stopped)
        return CHECK_ERROR(Runtime::setError("non-reentrant function"));

    s_state_stopped = true;

    Isolate* m_isolate = new Isolate("");
    syncCall(m_isolate, _thread::init_proc, new _thread());
    return 0;
}

result_t chain_base::get_name(exlib::string& retVal)
{
    retVal = chain::config::system_account_name.to_string();
    return 0;
}

result_t chain_base::set_name(exlib::string newVal)
{
    exlib::string n;

    n = newVal;
    chain::config::system_account_name = name(std::string_view { n.c_str(), n.length() });

    n = newVal + ".null";
    chain::config::null_account_name = name(std::string_view { n.c_str(), n.length() });

    n = newVal + ".prods";
    chain::config::producers_account_name = name(std::string_view { n.c_str(), n.length() });

    n = newVal + ".auth";
    chain::config::eosio_auth_scope = name(std::string_view { n.c_str(), n.length() });

    n = newVal + ".all";
    chain::config::eosio_all_scope = name(std::string_view { n.c_str(), n.length() });

    n = newVal + ".any";
    chain::config::eosio_any_name = name(std::string_view { n.c_str(), n.length() });

    n = newVal + ".code";
    chain::config::eosio_code_name = name(std::string_view { n.c_str(), n.length() });

    return 0;
}

result_t chain_base::get_data_dir(exlib::string& retVal)
{
    retVal = app().data_dir().c_str();
    return 0;
}

result_t chain_base::set_data_dir(exlib::string newVal)
{
    app().set_default_data_dir(newVal.c_str());
    return 0;
}

result_t chain_base::get_config_dir(exlib::string& retVal)
{
    retVal = app().config_dir().c_str();
    return 0;
}

result_t chain_base::set_config_dir(exlib::string newVal)
{
    app().set_default_config_dir(newVal.c_str());
    return 0;
}

result_t chain_base::get_log_level(exlib::string& retVal)
{
    switch (fc::logger::get().get_log_level()) {
    case fc::log_level::all:
        retVal = "all";
        break;
    case fc::log_level::debug:
        retVal = "debug";
        break;
    case fc::log_level::info:
        retVal = "info";
        break;
    case fc::log_level::warn:
        retVal = "warn";
        break;
    case fc::log_level::error:
        retVal = "error";
        break;
    case fc::log_level::off:
        retVal = "off";
        break;
    }

    return 0;
}

result_t chain_base::set_log_level(exlib::string newVal)
{
    auto logger = fc::logger::get();

    if (newVal == "all")
        logger.set_log_level(fc::log_level::all);
    else if (newVal == "debug")
        logger.set_log_level(fc::log_level::debug);
    else if (newVal == "info")
        logger.set_log_level(fc::log_level::info);
    else if (newVal == "warn")
        logger.set_log_level(fc::log_level::warn);
    else if (newVal == "error")
        logger.set_log_level(fc::log_level::error);
    else if (newVal == "off")
        logger.set_log_level(fc::log_level::off);
    else
        return CHECK_ERROR(CALL_E_INVALIDARG);

    return 0;
}

result_t chain_base::get_pubkey_prefix(exlib::string& retVal)
{
    retVal = fc::crypto::config::public_key_legacy_prefix;
    return 0;
}

result_t chain_base::set_pubkey_prefix(exlib::string newVal)
{
    static exlib::string s_prefix;

    s_prefix = newVal;
    fc::crypto::config::public_key_legacy_prefix = s_prefix.c_str();
    dmc_root_key = (newVal + base_root_key).c_str();

    return 0;
}

result_t chain_base::stop()
{
    if (s_state_started)
        return CHECK_ERROR(Runtime::setError("non-reentrant function"));

    s_state_started = true;
    app().quit();
    return 0;
}
} // namespace fibjs
