extern void *parsepubkey(const char *pubstr);
extern void freepubkey(void *pubkey);
extern void *genchallenge(void *pubkey, const void *seed, int seedlen, vector<char> &challengestr);
extern void freechallenge(void *answer);
extern bool checkchallenge(const char *answerstr, void *correct);
