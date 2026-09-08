// Developer-only regression fixture. Never compiled into the ROM.
static void add_builtin_names()
{
    add_name("builtin.txt");
    add_name("NL-DE-5000.txt");
    add_name("ES-DE-vocab.txt");
}

static void ensure_names_for_tests_or_fallback()
{
    if (s_name_count == 0) {
        add_builtin_names();
    }
}

static bool copy_text(const char* text, char* out, int out_len, int& out_used)
{
    if (!text || !out || out_len <= 0) return false;
    int len = 0;
    while (text[len] && len < out_len - 1) {
        out[len] = text[len];
        len++;
    }
    out[len] = 0;
    out_used = len;
    return text[len] == 0;
}

bool vocab_file_read_builtin_or_stub(const char* filename, char* out, int out_len, int& out_used)
{
    static const char* builtin =
        "English\tEnglish\n"
        "français\tFrench\n"
        "Deutsch\tGerman\n"
        "español\tSpanish\n"
        "português\tPortuguese\n"
        "italiano\tItalian\n"
        "svenska\tSwedish\n"
        "dansk\tDanish\n"
        "norsk\tNorwegian\n"
        "suomi\tFinnish\n"
        "íslenska\tIcelandic\n"
        "føroyskt\tFaroese\n"
        "Nederlands\tDutch\n"
        "polski\tPolish\n"
        "čeština\tCzech\n"
        "Türkçe\tTurkish\n"
        "Ελληνικά\tGreek\n"
        "русский\tRussian\n"
        "українська\tUkrainian\n"
        "日本語\tJapanese\n"
        "中文\tChinese\n"
        "한국어\tKorean\n";

    static const char* nl_de =
        "hond\tHund\r\n"
        "kat\tKatze\r\n"
        "\r\n"
        "boom\tBaum\r\n"
        "huis\tHaus\r\n"
        "\r\n"
        "boek\tBuch\r\n";

    static const char* es_de =
        "perro\tHund\r\n"
        "gato\tKatze\r\n"
        "agua\tWasser\r\n"
        "\r\n"
        "pan\tBrot\r\n"
        "escuela\tSchule\r\n";

    if (str_eq_local(filename, "builtin.txt")) return copy_text(builtin, out, out_len, out_used);
    if (str_eq_local(filename, "NL-DE-5000.txt")) return copy_text(nl_de, out, out_len, out_used);
    if (str_eq_local(filename, "ES-DE-vocab.txt")) return copy_text(es_de, out, out_len, out_used);
    return false;
}

