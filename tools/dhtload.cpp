#include "tools_common.h"
#include "opendht/dht_proxy_server.h"
#include "opendht/real_time.h"

#include <memory>

static dht::Value::Id
getSequentialId()
{
    static dht::Value::Id id = 0x3141590000000000;
    return ++id;
}

std::atomic<bool> running = true;

int main(int argc, char** argv)
{
    bool verbose = false;
    if (argc > 1 && std::string(argv[1]) == "-v") {
        verbose = true;
    }

    //
    // --- DHT configuration ---
    //
    constexpr int num_nodes = 3;
    constexpr in_port_t initial_port = 4000;

    std::vector<std::shared_ptr<dht::DhtRunner>> nodes;

    std::unique_ptr<dht::DhtProxyServer> proxy_server;
    dht::ProxyServerConfig serverConfig;

    //
    // --- Create and start nodes ---
    //
    fmt::print(stderr, "@@@ Starting {} nodes...\n", num_nodes);
    for (int i = 0; i < num_nodes; i++) {
        dht::DhtRunner::Config config;
        auto node_ca = std::make_unique<dht::crypto::Identity>(dht::crypto::generateEcIdentity(fmt::format("Node #{:03} CA", i)));
        config.dht_config.id = dht::crypto::generateIdentity(fmt::format("Node #{:03}", i), *node_ca);
        config.proxy_server = (i == 0) ? "" : fmt::format("localhost:{}", serverConfig.port);

        dht::DhtRunner::Context context;
        context.logger = (verbose && i == 0) ? dht::log::getStdLogger() : nullptr;
        //context.logger = dht::log::getFileLogger(fmt::format("./logs/node_{:03}.txt", i));

        nodes.emplace_back(std::make_shared<dht::DhtRunner>());
        auto& node = nodes.back();
        in_port_t port = initial_port + i;
        node->run(port, config, std::move(context));
        if (i == 0) {
            node->bootstrap("bootstrap.jami.net");
            proxy_server = std::make_unique<dht::DhtProxyServer>(node, std::make_unique<dht::Time>(), serverConfig);
        }
        fmt::print(stderr, "@@@ Node #{:03} running [Port = {}, Id = {}{}]\n",
                   i, port, node->getNodeId().toString(),
                   (i == 0) ? fmt::format(", Server port = {}", serverConfig.port) : "");
    }

    //
    // --- Test put & listen operations ---
    //
    fmt::print(stderr, "@@@ Make 'listen' operations...\n");
    assert(num_nodes >= 3);

    std::vector<dht::InfoHash> hashes;

    for (int i = 2; i < num_nodes; i++) {
        dht::InfoHash hash = dht::InfoHash::get(fmt::format("key_for_listener_{}_{}", i, std::time(nullptr)));
        hashes.push_back(hash);
        fmt::print(stderr, "@@@ Node #{:03} listening for {}\n", i, hash.toString());
        nodes[i]->listen(hash,
            [i](const std::vector<dht::Sp<dht::Value>>& values, bool expired) {
                std::string log = fmt::format("@@@  Node #{:03} listen: {} {} value{}",
                                              i,
                                              values.size(),
                                              expired ? "expired" : "new",
                                              values.size() > 1 ? "s" : "");
                char sep = ':';
                for (auto value : values) {
                    log += fmt::format("{} {:x}", sep, value->id);
                    sep = ',';
                }
                log += "\n";
                fmt::print(stderr, log);
                return true;
            }
        );
    }

    fmt::print(stderr, "@@@ Starting to put values on DHT...\n");
    auto& sender = nodes[1];
    auto put_thread = std::thread([sender, hashes]() {
        std::string value = "ABCD1234";
        while (running) {
            for (const auto& hash : hashes) {
                dht::Value val;
                val.id = getSequentialId();
                val.data = std::vector<uint8_t>(value.begin(), value.end());
                fmt::print(stderr, "@@@ Sender: putting value [id = {:x}] at hash {}\n",
                           val.id,
                           hash.toString());
                sender->put(hash, std::move(val),
                    [hash, id = val.id](bool ok) {
                        fmt::print(stderr, "@@@   Sender: put value [id = {:x}] at hash {} {}\n",
                                   id,
                                   hash.toString(),
                                   ok ? "succeeded" : "failed");
                    }
                );
                if (!running) break;
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(10));
    running = false;
    put_thread.join();

    //
    // --- Stop nodes ---
    //
    fmt::print(stderr, "@@@ Stopping nodes...\n");
    for (int i = 0; i < num_nodes; i++) {
        nodes[i]->join();
        fmt::print(stderr, "@@@ Node #{:03} stopped\n", i);
    }
    return 0;
}