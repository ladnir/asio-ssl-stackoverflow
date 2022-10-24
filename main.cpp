
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <vector>
#include <string>
#include <thread>
#include <iostream>

using u64 = unsigned long long;
using u8 = unsigned char;

int main()
{
    using namespace boost::asio;
    using namespace boost::asio::ip;

    boost::asio::io_context ioc;

    auto w = std::make_unique<boost::asio::io_context::work>(ioc);
    std::vector<std::thread> thrds(4);
    for (auto& t : thrds)
        t = std::thread([&] { ioc.run(); });

    int port = 1212;
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

    tcp::acceptor acceptor(ioc);
    tcp::resolver resolver(ioc);
    tcp::endpoint endpoint = *resolver.resolve("localhost", std::to_string(port).c_str());
    acceptor.open(endpoint.protocol());
    acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor.bind(endpoint);
    acceptor.listen(1);

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
    //std::vector<char> mData(10000);

    std::atomic<u64> c(0);

    std::function<void(bool, ssl::stream<tcp::socket>&, io_context::strand&, u64)> f =
        [&](bool send, ssl::stream<tcp::socket>& sock, io_context::strand& strand, u64 t) {
        ++c;
        strand.dispatch([&, send, t]() {
            std::vector<u8> buffer(10000);
            if (send)
            {
                auto bb = const_buffer(buffer.data(), buffer.size());
                async_write(sock, bb,
                    [&, send, t, buffer = std::move(buffer)](boost::system::error_code error, std::size_t n) mutable {
                        if (error)
                        {
                            std::cout << error.message() << std::endl;
                            std::terminate();
                        }
                        if (t)
                            f(send, sock, strand, t - 1);
                        --c;
                    });
            }
            else
            {
                auto bb = mutable_buffer(buffer.data(), buffer.size());
                async_read(sock, bb,
                    [&, send, t, buffer = std::move(buffer)](boost::system::error_code error, std::size_t n) mutable {
                        //callback(error, n/*, s*/);
                        if (error)
                        {
                            std::cout << error.message() << std::endl;
                            std::terminate();
                        }

                        if (t)
                            f(send, sock, strand, t - 1);
                        --c;
                    }
                );
            }
            });

    };

    f(true, srv, srvStrand, trials);
    f(false, srv, srvStrand, trials);
    f(true, cli, cliStrand, trials);
    f(false, cli, cliStrand, trials);

    while (c);
    w.reset();
    for (auto& t : thrds)
        t.join();

    return 0;
}
