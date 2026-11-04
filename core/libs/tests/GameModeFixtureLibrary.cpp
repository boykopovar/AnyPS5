namespace {

int startResult = 0;
int endResult = 0;
int starts = 0;
int ends = 0;

}

extern "C" {

int real_gamemode_request_start() {
    ++starts;
    return startResult;
}

#ifndef FIXTURE_OMIT_END
int real_gamemode_request_end() {
    ++ends;
    return endResult;
}
#endif

#ifndef FIXTURE_OMIT_ERROR_STRING
const char* real_gamemode_error_string() {
    return "fixture daemon is not running";
}
#endif

void fixture_configure(int start, int end) {
    startResult = start;
    endResult = end;
}

int fixture_starts() {
    return starts;
}

int fixture_ends() {
    return ends;
}

}
