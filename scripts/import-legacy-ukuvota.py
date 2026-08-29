#!/usr/bin/env python3
import argparse
import datetime as dt
import email.utils
import hashlib
import json
import os
import re
import sqlite3
import sys


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DEFAULT_DB = os.path.join(ROOT, "uku.sqlite3")


def now_ts():
    return int(dt.datetime.now(dt.timezone.utc).timestamp())


def strip_html(value):
    if value is None:
        return ""
    text = str(value)
    text = re.sub(r"<br\s*/?>", "\n", text, flags=re.I)
    text = re.sub(r"</p\s*>", "\n", text, flags=re.I)
    text = re.sub(r"<[^>]+>", "", text)
    return text.strip()


def parse_timestamp(value):
    if value is None or value == "":
        return None
    if isinstance(value, (int, float)):
        timestamp = float(value)
        if timestamp > 100000000000:
            timestamp = timestamp / 1000.0
        return int(timestamp)

    text = str(value).strip()
    if not text:
        return None

    if re.fullmatch(r"-?\d+(\.\d+)?", text):
        return parse_timestamp(float(text))

    iso = text
    if iso.endswith("Z"):
        iso = iso[:-1] + "+00:00"
    try:
        parsed = dt.datetime.fromisoformat(iso)
        if parsed.tzinfo is None:
            parsed = parsed.replace(tzinfo=dt.timezone.utc)
        return int(parsed.timestamp())
    except ValueError:
        pass

    try:
        parsed = email.utils.parsedate_to_datetime(text)
        if parsed is not None:
            if parsed.tzinfo is None:
                parsed = parsed.replace(tzinfo=dt.timezone.utc)
            return int(parsed.timestamp())
    except (TypeError, ValueError, IndexError):
        pass

    without_comment = re.sub(r"\s*\([^)]*\)\s*$", "", text)
    formats = [
        "%a %b %d %Y %H:%M:%S GMT%z",
        "%a %b %d %Y %H:%M:%S %z",
        "%a %b %d %H:%M:%S %Y %z",
        "%Y-%m-%d %H:%M:%S%z",
        "%Y-%m-%d %H:%M:%S",
        "%Y-%m-%d %H:%M",
    ]
    for fmt in formats:
        try:
            parsed = dt.datetime.strptime(without_comment, fmt)
            if parsed.tzinfo is None:
                parsed = parsed.replace(tzinfo=dt.timezone.utc)
            return int(parsed.timestamp())
        except ValueError:
            continue

    return None


def load_docs(path):
    with open(path, "r", encoding="utf-8") as handle:
        payload = json.load(handle)

    if isinstance(payload, list):
        return payload
    if isinstance(payload, dict):
        if isinstance(payload.get("rows"), list):
            return [row.get("doc", row) for row in payload["rows"] if isinstance(row, dict)]
        if isinstance(payload.get("docs"), list):
            return payload["docs"]
        if isinstance(payload.get("results"), list):
            return payload["results"]
        return [payload]
    raise ValueError("legacy export must be a JSON object or array")


def normalize_weight(value):
    if isinstance(value, str) and value.lower() == "infinity":
        return 9
    try:
        weight = int(value)
    except (TypeError, ValueError):
        return 3
    return max(1, min(weight, 9))


def normalize_phase(proposal_end, voting_end, created_at):
    current = now_ts()
    if proposal_end and current < proposal_end:
        return "collecting"
    if voting_end and current < voting_end:
        return "voting"
    if voting_end or proposal_end:
        return "completed"
    if created_at <= current:
        return "published"
    return "draft"


def iter_topic_docs(docs):
    for doc in docs:
        if not isinstance(doc, dict):
            continue
        if doc.get("_deleted"):
            continue
        if "question" not in doc and "proposals" not in doc and "votes" not in doc:
            continue
        yield doc


def ordered_items(value):
    if isinstance(value, dict):
        return list(value.items())
    if isinstance(value, list):
        items = []
        for index, item in enumerate(value):
            if isinstance(item, dict):
                item_id = item.get("id") or item.get("_id") or str(index)
                items.append((str(item_id), item))
        return items
    return []


def build_scores(vote, proposal_ids):
    if not isinstance(vote, dict):
        return ""
    parts = []
    known = set(proposal_ids)
    for proposal_id, score in vote.items():
        proposal_id = str(proposal_id)
        if known and proposal_id not in known:
            continue
        try:
            value = int(score)
        except (TypeError, ValueError):
            continue
        value = max(-3, min(value, 3))
        parts.append("{}={}".format(proposal_id, value))
    return ";".join(parts)


def voter_id(name):
    digest = hashlib.sha256(str(name).encode("utf-8")).hexdigest()[:32]
    return "legacy:{}".format(digest)


def ensure_schema(conn):
    conn.executescript(
        """
        create table if not exists processes(
            id text primary key,
            type text not null,
            phase text not null,
            topic text not null,
            description text not null,
            proposal_minutes integer not null,
            voting_minutes integer not null,
            negative_weight integer not null,
            visibility text not null default 'public',
            local_address text not null,
            created_at integer not null,
            synced integer not null default 0
        );
        create table if not exists proposals(
            id integer primary key autoincrement,
            process_id text not null,
            author_user_id text not null default '',
            remote_id text not null default '',
            title text not null,
            description text not null,
            created_at integer not null,
            synced integer not null default 0
        );
        create table if not exists votes(
            process_id text not null,
            voter_user_id text not null,
            display_name text not null,
            reason text not null,
            scores text not null,
            updated_at integer not null,
            synced integer not null default 0,
            primary key(process_id, voter_user_id)
        );
        create table if not exists options(
            id text primary key,
            process_id text not null,
            label text not null,
            description text not null,
            position integer not null
        );
        create table if not exists results(
            process_id text primary key,
            summary text not null,
            generated_at integer not null
        );
        """
    )


def delete_process(conn, process_id):
    for table in ("votes", "proposals", "options", "results"):
        conn.execute("delete from {} where process_id=?".format(table), (process_id,))
    conn.execute("delete from processes where id=?", (process_id,))


def import_doc(conn, doc, replace=False, dry_run=False):
    process_id = str(doc.get("_id") or doc.get("id") or "").strip()
    if not process_id:
        process_id = hashlib.sha256(json.dumps(doc, sort_keys=True).encode("utf-8")).hexdigest()[:24]

    existing = conn.execute("select 1 from processes where id=?", (process_id,)).fetchone()
    if existing and not replace:
        return {"id": process_id, "status": "skipped", "proposals": 0, "votes": 0}

    proposal_end = parse_timestamp(doc.get("proposalTime") or doc.get("proposal_time"))
    voting_end = parse_timestamp(doc.get("votingTime") or doc.get("voting_time"))
    created_at = parse_timestamp(doc.get("createdAt") or doc.get("created_at") or doc.get("created"))

    if created_at is None:
        if proposal_end is not None:
            created_at = proposal_end - 60
        elif voting_end is not None:
            created_at = voting_end - 3600
        else:
            created_at = now_ts()

    if proposal_end is None:
        proposal_end = created_at + 60
    if voting_end is None:
        voting_end = max(proposal_end + 60, created_at + 3600)

    proposal_minutes = max(1, int(round((proposal_end - created_at) / 60.0)))
    voting_minutes = max(1, int(round((voting_end - proposal_end) / 60.0)))
    topic = strip_html(doc.get("question") or doc.get("topic") or "Imported Ukuvota process")
    description = strip_html(doc.get("description") or "")
    negative_weight = normalize_weight(doc.get("negativeScoreWeight"))
    phase = normalize_phase(proposal_end, voting_end, created_at)
    local_address = "/app/{}/collect".format(process_id)

    proposal_items = ordered_items(doc.get("proposals"))
    proposal_ids = [str(key) for key, _ in proposal_items]
    vote_items = ordered_items(doc.get("votes"))

    if dry_run:
        return {
            "id": process_id,
            "status": "replace" if existing else "insert",
            "proposals": len(proposal_items),
            "votes": len(vote_items),
        }

    if existing and replace:
        delete_process(conn, process_id)

    conn.execute(
        """
        insert into processes(
            id, type, phase, topic, description, proposal_minutes, voting_minutes,
            negative_weight, visibility, local_address, created_at, synced
        ) values(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            process_id,
            "consent",
            phase,
            topic,
            description,
            proposal_minutes,
            voting_minutes,
            negative_weight,
            "public",
            local_address,
            created_at,
            1,
        ),
    )

    for position, (proposal_id, proposal) in enumerate(proposal_items):
        if not isinstance(proposal, dict):
            proposal = {"title": str(proposal), "description": ""}
        title = strip_html(proposal.get("title") or proposal_id)
        body = strip_html(proposal.get("description") or proposal.get("body") or "")
        conn.execute(
            """
            insert into proposals(process_id, author_user_id, remote_id, title, description, created_at, synced)
            values(?, ?, ?, ?, ?, ?, ?)
            """,
            (process_id, "legacy-import", str(proposal_id), title, body, created_at + position, 1),
        )

    imported_votes = 0
    for name, vote in vote_items:
        display_name = strip_html(name) or "Legacy voter"
        scores = build_scores(vote, proposal_ids)
        if not scores:
            continue
        conn.execute(
            """
            insert or replace into votes(process_id, voter_user_id, display_name, reason, scores, updated_at, synced)
            values(?, ?, ?, ?, ?, ?, ?)
            """,
            (process_id, voter_id(display_name), display_name, "", scores, voting_end, 1),
        )
        imported_votes += 1

    return {
        "id": process_id,
        "status": "replaced" if existing else "inserted",
        "proposals": len(proposal_items),
        "votes": imported_votes,
    }


def main():
    parser = argparse.ArgumentParser(
        description="One-time import of legacy Ukuvota PouchDB topic JSON into Uku SQLite."
    )
    parser.add_argument("input", help="Legacy JSON export, Pouch _all_docs output, or one topic document.")
    parser.add_argument("--sqlite", default=DEFAULT_DB, help="Target Uku SQLite database.")
    parser.add_argument("--replace", action="store_true", help="Replace processes whose ids already exist.")
    parser.add_argument("--dry-run", action="store_true", help="Show what would be imported without writing.")
    args = parser.parse_args()

    docs = list(iter_topic_docs(load_docs(args.input)))
    if not docs:
        print("No legacy Ukuvota topic documents found in {}".format(args.input), file=sys.stderr)
        return 1

    conn = sqlite3.connect(args.sqlite)
    try:
        ensure_schema(conn)
        total_proposals = 0
        total_votes = 0
        inserted = 0
        skipped = 0
        with conn:
            for doc in docs:
                result = import_doc(conn, doc, replace=args.replace, dry_run=args.dry_run)
                total_proposals += result["proposals"]
                total_votes += result["votes"]
                if result["status"] == "skipped":
                    skipped += 1
                else:
                    inserted += 1
                print(
                    "{status}: {id} ({proposals} proposals, {votes} votes)".format(**result)
                )

        action = "Would import" if args.dry_run else "Imported"
        print(
            "{} {} process(es), {} proposal(s), {} vote(s); skipped {}.".format(
                action, inserted, total_proposals, total_votes, skipped
            )
        )
    finally:
        conn.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
