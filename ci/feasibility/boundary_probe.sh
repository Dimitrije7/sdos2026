#!/usr/bin/env bash
# Task 005 probe: confirm the workflow token is read-only and that no OIDC
# identity token is issued, then report the event context as JSON.
# Needs GH_TOKEN, GITHUB_REPOSITORY, GITHUB_SHA, GITHUB_RUN_ID, GITHUB_EVENT_PATH.
set -euo pipefail

[[ $# -eq 1 ]] || {
  echo "usage: $0 <summary.json>" >&2
  exit 2
}
summary=$1
: "${GH_TOKEN:?}" "${GITHUB_REPOSITORY:?}" "${GITHUB_SHA:?}" "${GITHUB_RUN_ID:?}"

oidc_available=false
[[ -n ${ACTIONS_ID_TOKEN_REQUEST_URL:-} ]] && oidc_available=true

# A read-only token must be refused when it tries to create a tag.
probe_ref="refs/tags/sdos-boundary-probe-$GITHUB_RUN_ID"
status=$(curl --silent --output /dev/null --write-out '%{http_code}' \
  --request POST \
  --header "Authorization: Bearer $GH_TOKEN" \
  --header "Accept: application/vnd.github+json" \
  --data "{\"ref\":\"$probe_ref\",\"sha\":\"$GITHUB_SHA\"}" \
  "https://api.github.com/repos/$GITHUB_REPOSITORY/git/refs")
if [[ $status == 201 ]]; then
  curl --silent --output /dev/null --request DELETE \
    --header "Authorization: Bearer $GH_TOKEN" \
    "https://api.github.com/repos/$GITHUB_REPOSITORY/git/$probe_ref"
  echo "the workflow token could write a tag; the boundary is broken" >&2
  exit 1
fi
if [[ $status != 403 && $status != 404 ]]; then
  echo "unexpected HTTP $status from the write probe; cannot prove read-only" >&2
  exit 1
fi
if [[ $oidc_available == true ]]; then
  echo "an OIDC token request URL is present; id-token permission leaked" >&2
  exit 1
fi

STATUS=$status OIDC=$oidc_available python3 - "$summary" <<'EOF'
import json, os, sys
env = os.environ
event = {}
if os.path.exists(env.get("GITHUB_EVENT_PATH", "")):
    with open(env["GITHUB_EVENT_PATH"], encoding="utf-8") as handle:
        event = json.load(handle)
head_repo = (event.get("pull_request") or {}).get("head", {}).get("repo") or {}
summary = {
    "probe": "token-boundary",
    "event_name": env.get("GITHUB_EVENT_NAME"),
    "from_fork": bool(head_repo.get("fork")),
    "tag_write_http_status": int(env["STATUS"]),
    "token_write_refused": env["STATUS"] in {"403", "404"},
    "oidc_token_available": env["OIDC"] == "true",
    "runner_image": env.get("ImageOS", "") + " " + env.get("ImageVersion", ""),
}
with open(sys.argv[1], "w", encoding="utf-8") as handle:
    json.dump(summary, handle, indent=2)
    handle.write("\n")
print(json.dumps(summary, indent=2))
EOF
