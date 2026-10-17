thread_local long tls_initial = 7;

extern "C" long tls_initial_bump() {
    tls_initial += 2;
    return tls_initial;
}
