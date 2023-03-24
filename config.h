static const char datecmd[] = "date '+%x %r'";
static const char hostnamecmd[] = "hostname";
static const char ramcmd[] = "free -h | awk 'FNR == 2 {print $3 \"/\"$2}'";
static const char cpucmd[] = "vmstat | awk 'FNR == 3 {print 100-$15\"%\"}'";
static const char tempcmd[] = "sensors | sed -En 's/^temp.: *\\+(.*C) /\\1/p' | awk 'FNR == 1' | sed -En 's/crit = \\+//p'";
static const char audiocmd[] = "pamixer --get-volume | awk '{print $1\"%\"}'";
#define seperator " | "

#define SECONDS(n) (1 * 1000)
#define MINUTES(n) (SECONDS(n) * 60)

// {prefix, command, ms_interval}
static const Module modules[] = {
    {NULL, hostnamecmd, 0},
    {NULL, datecmd, SECONDS(1)},
    {"RAM: ", ramcmd, MINUTES(1.5)},
		{"CPU: ", cpucmd, MINUTES(0.5)},
		{"TEMP: ", tempcmd, MINUTES(0.5)},
		{"VOL: ", audiocmd, SECONDS(0.5)},
};
