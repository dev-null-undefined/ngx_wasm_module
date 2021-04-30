# vim:set ft= ts=4 sw=4 et fdm=marker:

use strict;
use lib '.';
use t::TestWasm;

skip_valgrind();

#repeat_each(2);

plan tests => repeat_each() * (blocks() * 7);

run_tests();

__DATA__

=== TEST 1: proxy_wasm - on_request_headers logs number of request headers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? \[wasm\] #\d+ on_request_headers, 2 headers/
--- no_error_log
[error]
[emerg]
[alert]
[crit]



=== TEST 2: proxy_wasm - on_response_headers logs number of response headers
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        echo ok;
    }
--- ignore_response_body
--- response_headers_like
Server: \S+
Date: [\S\s]+
Content-Type: \S+
Transfer-Encoding: chunked
Connection: close
--- error_log eval
qr/\[info\] .*? \[wasm\] #\d+ on_response_headers, 5 headers/



=== TEST 3: proxy_wasm - on_log
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        echo ok;
    }
--- response_body
ok
--- error_log eval
qr/\[info\] .*? \[wasm\] #\d+ on_log/
--- no_error_log
[error]
[emerg]
[alert]
[crit]



=== TEST 4: proxy_wasm - missing default content handler
should cause HTTP 404 from static module (default content handler)
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        echo_status 201;
    }
--- error_code: 404
--- response_body eval
qr/404 Not Found/
--- error_log eval
[
    qr/\[error\] .*? open\(\) \".*?\" failed/,
    qr/\[info\] .*? \[wasm\] #\d+ on_request_headers/,
    qr/\[info\] .*? \[wasm\] #\d+ on_response_headers/,
    qr/\[info\] .*? \[wasm\] #\d+ on_log/
]
--- no_error_log
[crit]



=== TEST 5: proxy_wasm - with 'return' (rewrite)
should produce a response in and of itself, proxy_wasm wraps around
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        return 201;
    }
--- error_code: 201
--- response_body
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] #\d+ on_request_headers/,
    qr/\[info\] .*? \[wasm\] #\d+ on_response_headers/,
    qr/\[info\] .*? \[wasm\] #\d+ on_log/
]
--- no_error_log
[error]
[crit]



=== TEST 6: proxy_wasm - before content producer 'echo'
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        echo ok;
    }
--- error_code: 200
--- response_body
ok
--- error_log eval
--- no_error_log
[error]
[crit]
[emerg]
[alert]
[stderr]



=== TEST 7: proxy_wasm - after content producer 'echo'
should produce a response from echo, even if proxy_wasm was added
below it, it should wrap around echo
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /t {
        echo ok;
        proxy_wasm on_phases;
    }
--- response_body
ok
--- error_log eval
--- no_error_log
[error]
[crit]
[emerg]
[alert]
[stderr]



=== TEST 8: proxy_wasm - before content producer 'proxy_pass'
should produce a response from proxy_pass, proxy_wasm wraps around
--- wasm_modules: on_phases
--- http_config eval
qq{
    upstream test_upstream {
        server unix:$ENV{TEST_NGINX_UNIX_SOCKET};
    }

    server {
        listen unix:$ENV{TEST_NGINX_UNIX_SOCKET};

        location / {
            return 201;
        }
    }
}
--- config
    location /t {
        proxy_wasm on_phases;
        proxy_pass http://test_upstream/;
    }
--- error_code: 201
--- response_body
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] #\d+ on_request_headers/,
    qr/\[info\] .*? \[wasm\] #\d+ on_response_headers/,
    qr/\[info\] .*? \[wasm\] #\d+ on_log/
]
--- no_error_log
[error]
[crit]



=== TEST 9: proxy_wasm - as a subrequest
should not execute a log phase
--- wasm_modules: on_phases
--- load_nginx_modules: ngx_http_echo_module
--- config
    location /subrequest {
        internal;
        proxy_wasm on_phases;
        return 201;
    }

    location /t {
        echo_subrequest GET '/subrequest';
    }
--- error_code: 200
--- response_body
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] #\d+ on_request_headers, \d+ headers .*? subrequest: "\/subrequest"/,
    qr/\[info\] .*? \[wasm\] #\d+ on_response_headers, \d+ headers .*? subrequest: "\/subrequest"/,
]
--- no_error_log eval
[
    qr/on_log .*? subrequest: "\/subrequest"/,
    qr/\[error\]/,
    qr/\[crit\]/
]



=== TEST 10: proxy_wasm - same module in multiple location{} blocks
--- load_nginx_modules: ngx_http_echo_module
--- wasm_modules: on_phases
--- config
    location /subrequest/a {
        proxy_wasm on_phases;
        echo A;
    }

    location /subrequest/b {
        proxy_wasm on_phases;
        echo B;
    }

    location /t {
        echo_subrequest GET '/subrequest/a';
        echo_subrequest GET '/subrequest/b';
    }
--- error_code: 200
--- response_body
A
B
--- error_log eval
[
    qr/\[info\] .*? \[wasm\] #\d+ on_request_headers, \d+ headers .*? subrequest: "\/subrequest\/a"/,
    qr/\[info\] .*? \[wasm\] #\d+ on_response_headers, \d+ headers .*? subrequest: "\/subrequest\/a"/,
    qr/\[info\] .*? \[wasm\] #\d+ on_request_headers, \d+ headers .*? subrequest: "\/subrequest\/b"/,
    qr/\[info\] .*? \[wasm\] #\d+ on_response_headers, \d+ headers .*? subrequest: "\/subrequest\/b"/,
]
--- no_error_log
[error]



=== TEST 11: proxy_wasm - chained filters in same location{} block
should run each filter after the other within each phase
--- skip_no_debug: 7
--- wasm_modules: on_phases
--- config
    location /t {
        proxy_wasm on_phases;
        proxy_wasm on_phases;
        return 200;
    }
--- grep_error_log eval: qr/\[wasm\] #\d+ on_(request|response|log).*?$/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_request_headers, \d+ headers .*?
\[wasm\] #\d+ on_request_headers, \d+ headers .*?
\[wasm\] #\d+ on_response_headers, \d+ headers .*?
\[wasm\] #\d+ on_response_headers, \d+ headers .*?
\[wasm\] #\d+ on_log .*?
\[wasm\] #\d+ on_log .*?
/
--- error_log eval
[
    qr/\[debug\] .*? wasm ops resuming \"header_filter\" phase \(idx: \d+, nops: \d+, force_ops: 1\)/,
    qr/\[debug\] .*? wasm ops resuming \"log\" phase \(idx: \d+, nops: \d+, force_ops: 1\)/
]
--- no_error_log
[error]
[crit]
[emerg]



=== TEST 12: proxy_wasm - chained filters in server{} block
should run each filter after the other within each phase
--- wasm_modules: on_phases
--- config
    proxy_wasm on_phases;
    proxy_wasm on_phases;

    location /t {
        return 200;
    }
--- grep_error_log eval: qr/\[wasm\] #\d+ on_(request|response|log).*?$/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_request_headers, \d+ headers .*?
\[wasm\] #\d+ on_request_headers, \d+ headers .*?
\[wasm\] #\d+ on_response_headers, \d+ headers .*?
\[wasm\] #\d+ on_response_headers, \d+ headers .*?
\[wasm\] #\d+ on_log .*?
\[wasm\] #\d+ on_log .*?
/
--- no_error_log
[error]
[crit]
[emerg]
[alert]
[stderr]



=== TEST 13: proxy_wasm - chained filters in http{} block
should run each filter after the other within each phase
--- wasm_modules: on_phases
--- http_config
    proxy_wasm on_phases;
    proxy_wasm on_phases;
--- config
    location /t {
        return 200;
    }
--- grep_error_log eval: qr/\[wasm\] #\d+ on_(request|response|log).*?$/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_request_headers, \d+ headers .*?
\[wasm\] #\d+ on_request_headers, \d+ headers .*?
\[wasm\] #\d+ on_response_headers, \d+ headers .*?
\[wasm\] #\d+ on_response_headers, \d+ headers .*?
\[wasm\] #\d+ on_log .*?
\[wasm\] #\d+ on_log .*?
/
--- no_error_log
[error]
[crit]
[emerg]
[alert]
[stderr]



=== TEST 14: proxy_wasm - mixed filters in server{} and http{} blocks
should not chain; instead, server{} overrides http{}
--- wasm_modules: on_phases
--- http_config
    proxy_wasm on_phases 'log_msg=http';
--- config
    proxy_wasm on_phases 'log_msg=server';

    location /t {
        return 200;
    }
--- grep_error_log eval: qr/\[wasm\] #\d+ on_(request|response|log).*?$/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_request_headers, \d+ headers .*?
\[wasm\] #\d+ on_response_headers, \d+ headers .*?
\[wasm\] #\d+ on_log .*?
/
--- error_log eval
qr/log_msg: server .*? request: "GET \/t\s+/
--- no_error_log eval
[
    qr/log_msg: http .*? request: "GET \/t\s+/,
    qr/\[error\]/,
    qr/\[crit\]/,
    qr/\[emerg\]/
]



=== TEST 15: proxy_wasm - mixed filters in server{} and location{} blocks
should not chain; instead, location{} overrides server{}
--- wasm_modules: on_phases
--- config
    proxy_wasm on_phases 'log_msg=server';

    location /t {
        proxy_wasm on_phases 'log_msg=location';
        return 200;
    }
--- grep_error_log eval: qr/\[wasm\] #\d+ on_(request|response|log).*?$/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_request_headers, \d+ headers .*?
\[wasm\] #\d+ on_response_headers, \d+ headers .*?
\[wasm\] #\d+ on_log .*?
/
--- error_log eval
qr/log_msg: location .*? request: "GET \/t\s+/
--- no_error_log eval
[
    qr/log_msg: server .*? request: "GET \/t\s+/,
    qr/\[error\]/,
    qr/\[crit\]/,
    qr/\[emerg\]/
]



=== TEST 16: proxy_wasm - mixed filters in http{}, server{}, and location{} blocks
should not chain; instead, location{} overrides server{}, server{} overrides http{}
--- wasm_modules: on_phases
--- http_config
    proxy_wasm on_phases 'log_msg=http';
--- config
    proxy_wasm on_phases 'log_msg=server';

    location /t {
        proxy_wasm on_phases 'log_msg=location';
        return 200;
    }
--- grep_error_log eval: qr/\[wasm\] #\d+ on_(request|response|log).*?$/
--- grep_error_log_out eval
qr/\[wasm\] #\d+ on_request_headers, \d+ headers .*?
\[wasm\] #\d+ on_response_headers, \d+ headers .*?
\[wasm\] #\d+ on_log .*?
/
--- error_log eval
qr/log_msg: location .*? request: "GET \/t\s+/
--- no_error_log eval
[
    qr/log_msg: server .*? request: "GET \/t\s+/,
    qr/log_msg: http .*? request: "GET \/t\s+/,
    qr/\[error\]/,
    qr/\[crit\]/
]