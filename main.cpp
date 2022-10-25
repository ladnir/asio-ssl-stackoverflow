
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <vector>
#include <string>
#include <thread>
#include <iostream>
#include <functional>

using u64 = unsigned long long;
using u8 = unsigned char;
static constexpr auto bufferSize = 100;


// This is the fixed version.
int main()
{
    using namespace boost::asio;
    using namespace boost::asio::ip;

    //create io context and threads.
    boost::asio::io_context ioc;
    auto w = std::make_unique<boost::asio::io_context::work>(ioc);
    std::vector<std::thread> thrds(4);
    for (auto& t : thrds)
        t = std::thread([&] { ioc.run(); });
    
    // setup ssl contex
    boost::asio::ssl::context sctx(boost::asio::ssl::context::tlsv13_server);
    boost::asio::ssl::context cctx(boost::asio::ssl::context::tlsv13_client);
    std::string dir = "./cert";
    auto file = dir + "/ca.cert.pem";
    sctx.load_verify_file(file);
    sctx.use_private_key_file(dir + "/server-0.key.pem", boost::asio::ssl::context::file_format::pem);
    sctx.use_certificate_file(dir + "/server-0.cert.pem", boost::asio::ssl::context::file_format::pem);

    //bind localhost:1212
    int port = 1212;
    tcp::acceptor acceptor(make_strand(ioc));
    tcp::resolver resolver(make_strand(ioc));
    tcp::endpoint endpoint = *resolver.resolve("localhost", std::to_string(port).c_str());
    acceptor.open(endpoint.protocol());
    acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor.bind(endpoint);
    acceptor.listen(1);

    // connect the sockets
    ssl::stream<tcp::socket> srv(make_strand(ioc), sctx), cli(make_strand(ioc), cctx);
    auto fu = std::async([&] {
        acceptor.accept(srv.lowest_layer());
        srv.handshake(ssl::stream_base::server);
        });
    connect(cli.lowest_layer(), &endpoint, &endpoint + 1);
    cli.handshake(ssl::stream_base::client);
    fu.get();

    u64 trials = 100;
    std::atomic<u64> spinLock(4);

    // this function launches a callback chain where
    // data is send and received `trial` number of times.
    std::function<void(bool, ssl::stream<tcp::socket>&, u64, std::unique_ptr<u8[]>)> f =
        [&f,&spinLock](bool send, ssl::stream<tcp::socket>& sock,  u64 t, std::unique_ptr<u8[]> buffer) {
        
        // within the strand perform an async_read or async_write
        if(!buffer)
            buffer.reset(new u8[bufferSize]);

        auto bb = mutable_buffer(buffer.get(), bufferSize);
        auto callback = [&f,&spinLock,&sock, send, t, buffer = std::move(buffer)](boost::system::error_code error, std::size_t n) mutable {
                if (error) {
                    std::cout<< t << ", " << error.message() << std::endl;
                    std::terminate();
                }

                // perform another operation or complete.
                if (t)
                    f(send, sock, t - 1, std::move(buffer));
                else
                    --spinLock;
            };

        if (send)
            async_write(sock, bb, std::move(callback));
        else
            async_read(sock, bb, std::move(callback));

    };

    // launch our callback chains.
    post(srv.get_executor(), [&](){ f(true, srv, trials, nullptr);});
    post(srv.get_executor(), [&](){ f(false, srv, trials, nullptr);});
    post(cli.get_executor(), [&](){ f(true, cli, trials,  nullptr);});
    post(cli.get_executor(), [&](){ f(false, cli, trials, nullptr);});
    
    // wait for them to complete and cleanup. 
    while (spinLock);
    w.reset();
    for (auto& t : thrds)
        t.join();

    return 0;
}

int original()
{
    using namespace boost::asio;
    using namespace boost::asio::ip;

    // create io context and threads.
    boost::asio::io_context ioc;
    auto w = std::make_unique<boost::asio::io_context::work>(ioc);
    std::vector<std::thread> thrds(4);
    for (auto& t : thrds)
        t = std::thread([&] { ioc.run(); });


    // setup ssl contex
    boost::asio::ssl::context serverCtx(boost::asio::ssl::context::tlsv13_server);
    boost::asio::ssl::context clientCtx(boost::asio::ssl::context::tlsv13_client);
    std::string dir = "./cert";
    auto file = dir + "/ca.cert.pem";
    serverCtx.load_verify_file(file);
    clientCtx.load_verify_file(file);
    clientCtx.use_private_key_file(dir + "/client-0.key.pem", boost::asio::ssl::context::file_format::pem);
    clientCtx.use_certificate_file(dir + "/client-0.cert.pem", boost::asio::ssl::context::file_format::pem);
    serverCtx.use_private_key_file(dir + "/server-0.key.pem", boost::asio::ssl::context::file_format::pem);
    serverCtx.use_certificate_file(dir + "/server-0.cert.pem", boost::asio::ssl::context::file_format::pem);

    // bind localhost:1212
    int port = 1212;
    tcp::acceptor acceptor(ioc);
    tcp::resolver resolver(ioc);
    tcp::endpoint endpoint = *resolver.resolve("localhost", std::to_string(port).c_str());
    acceptor.open(endpoint.protocol());
    acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor.bind(endpoint);
    acceptor.listen(1);

    // connect the sockets
    ssl::stream<tcp::socket> srv(ioc, serverCtx), cli(ioc, clientCtx);
    auto fu = std::async([&] {
        acceptor.accept(srv.lowest_layer());
        srv.handshake(ssl::stream_base::server);
        });
    connect(cli.lowest_layer(), &endpoint, &endpoint + 1);
    cli.handshake(ssl::stream_base::client);
    fu.get();

    io_context::strand srvStrand(ioc), cliStrand(ioc);
    u64 trials = 100;
    std::atomic<u64> spinLock(4);

    // this function launches a callback chain where
    // data is send and received `trial` number of times.
    std::function<void(bool, ssl::stream<tcp::socket>&, io_context::strand&, u64)> f =
        [&](bool send, ssl::stream<tcp::socket>& sock, io_context::strand& strand, u64 t) {
        
        // within the strand perform an async_read or async_write
        strand.dispatch([&, send, t]() {
            std::vector<u8> buffer(10000);
            auto bb = mutable_buffer(buffer.data(), buffer.size());
            auto callback = [&, send, t, buffer = std::move(buffer), moveOnly = std::unique_ptr<int>{}](boost::system::error_code error, std::size_t n) mutable {
                    if (error) {
                        std::cout << error.message() << std::endl;
                        std::terminate();
                    }

                    // perform another operation or complete.
                    if (t)
                        f(send, sock, strand, t - 1);
                    else
                        --spinLock;
                };

            if (send)
                async_write(sock, bb, std::move(callback));
            else
                async_read(sock, bb, std::move(callback));

        });
    };

    // launch our callback chains.
    f(true, srv, srvStrand, trials);
    f(false, srv, srvStrand, trials);
    f(true, cli, cliStrand, trials);
    f(false, cli, cliStrand, trials);

    // wait for them to complete and cleanup.
    while (spinLock);
    w.reset();
    for (auto& t : thrds)
        t.join();

    return 0;
}
