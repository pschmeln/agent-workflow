# Collaboration workflow

MakeUC Hackathon 2026. This repository is the shared coordination space for the owner, one human teammate, Grok Bot in Cursor, and Claude Code. No product, stack, or app is chosen. Add application code, dependencies, or product requirements only after a human writes them in a task.

Cursor loads a short operating card in [`AGENTS.md`](../AGENTS.md). Claude Code loads this guide through the import in [`CLAUDE.md`](../CLAUDE.md).

## Roles

| Role | Responsibility |
| --- | --- |
| Repository owner | Accepts tasks, reviews, and merges. Invites the teammate, and enables branch protection in GitHub settings when they choose to. |
| Human teammate | Uses their own GitHub, Git, Cursor, and Claude accounts. Pushes one branch per task and opens a pull request. |
| Grok Bot | Cursor agent that coordinates task text and does Cursor coding work, including cloud agents. Opens a pull request and waits for a human to merge. |
| Claude Code | Implements or reviews a task in a session a human starts. Opens a pull request and waits for a human to merge. |

## Separate sessions

Grok Bot and Claude Code run in separate sessions. A human starts Claude Code and pastes the task or the handoff. No automatic bridge from Grok Bot to Claude Code is configured, and none is assumed.

Installing Cursor or Claude Code, and signing in, are steps each person completes on their own account. This repository does not record those steps as done.

## Task ownership

- A human opens a task with [`.github/ISSUE_TEMPLATE/task.md`](../.github/ISSUE_TEMPLATE/task.md). An agent opens an issue only when the human in that session explicitly asks.
- One task has one owner and one branch. The owner is the only person or agent who pushes that branch.
- Name the owner with a real GitHub login, or with "Cursor agent" or "Claude Code session" plus the human who will start that session.
- The acceptance criteria on the task are the definition of done. Work that still misses them stays on the pull request.
- A handoff updates the task with what is done, what remains, the branch name, the pull request link, and the note the next human pastes into the next session.
- Keep `CODEOWNERS` out of the repo until a human supplies the teammate's GitHub username. No username is recorded here.

## Branches and worktrees

`main` is the integration branch. Commit on a task branch, then open a pull request.

Name the branch with the worker and a short topic: `owner/short-topic`, `teammate/short-topic`, `cursor/short-topic`, or `claude/short-topic`. One task uses one branch. Two workers use two branches.

A Cursor cloud agent already has its own virtual machine. It still needs its own branch. A person with Cursor access starts it from Cursor. See [Cloud Agents](https://cursor.com/docs/cloud-agent).

On one local clone, people and agents who work at the same time use separate Git worktrees. A branch stays in one worktree. This workflow does not force a second checkout of the same branch. The command reference is [git-worktree](https://git-scm.com/docs/git-worktree).

These commands fetch `main`, show existing worktrees, and add two new worktrees with new branches. Forward slashes work with Git on Linux, macOS, and Git for Windows. `git -C` runs Git in that directory without a shell-specific change of directory.

```bash
git fetch origin
git worktree list
git worktree add -b cursor/task-12 ../agent-workflow-cursor-task-12 origin/main
git worktree add -b claude/task-13 ../agent-workflow-claude-task-13 origin/main
git worktree list
git -C ../agent-workflow-cursor-task-12 status
git -C ../agent-workflow-cursor-task-12 push -u origin cursor/task-12
```

Publish the branch you mean to publish. Review `git status` in that worktree before each commit. Leave the other worktree alone. If the branch name already exists, pick a new name. The add commands create a worktree and a branch. The push publishes that branch. None of these commands rewrite or delete history.

## Pull requests

Keep each pull request to one task. A reviewer should be able to check the diff against that task's acceptance criteria.

GitHub fills in [`.github/PULL_REQUEST_TEMPLATE.md`](../.github/PULL_REQUEST_TEMPLATE.md) for new pull requests once the template is on the default branch, `main`.

In the pull request, name the review steps and the command you ran. The automated check in this repository validates workflow files. It is not an application test suite.

The author opens the pull request and stops. A human merges. Prefer approval from a human who did not author the change. When the other person is unavailable, the repository owner may merge and records that on the pull request. An agent is not an approver.

The template is a checklist. [Documentation and enforcement](#documentation-and-enforcement) records what GitHub enforces before an admin changes settings.

## Secrets

Keep passwords, tokens, private keys, `.env` files, and credential files out of Git, issues, pull requests, and docs. Store secrets in the environment or secret store of the tool that needs them, on that person's own account.

[`.gitignore`](../.gitignore) ignores `.env`, common private-key filenames, `CLAUDE.local.md`, and `.claude/settings.local.json`. A gitignore entry applies to untracked files. A file that is already committed stays in history until a human removes it on purpose.

## Teammate access

The teammate's GitHub username is not in this repository. These files send no invite.

The repository is public, so anyone can clone it. Push access starts after the owner invites that GitHub account and the teammate accepts. Until then, the teammate does not push with the owner's credentials.

The owner invites them from the repository access settings after they provide their GitHub username. Official steps: [Inviting collaborators to a personal repository](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/repository-access-and-collaboration/inviting-collaborators-to-a-personal-repository).

After they accept, while logged in as themselves:

1. Clone with their GitHub login: `git clone https://github.com/pschmeln/agent-workflow.git`
2. Or use an SSH key registered on their GitHub account: [Connecting to GitHub with SSH](https://docs.github.com/en/authentication/connecting-to-github-with-ssh).
3. For HTTPS with a personal access token, they create the token on their account: [Managing personal access tokens](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/managing-your-personal-access-tokens).
4. They sign in to Cursor and to Claude Code with their own accounts. GitHub access does not sign those products in.
5. They set their own `user.name` and `user.email` in Git on their machine. The repository does not store a shared identity.

The owner chooses the role GitHub offers on the invite. The teammate needs permission to push branches and open pull requests.

## Documentation and enforcement

| Rule | Current enforcement |
| --- | --- |
| A human merges to `main` | Written in this guide and the pull request template. People with write access can still merge. |
| Agents leave the merge to a human | Written in `AGENTS.md` and `CLAUDE.md`. |
| The scaffolding check passes | GitHub Actions runs the check. A failing check still allows merge until an admin requires that check. |
| Secrets stay out of Git | `.gitignore` plus review. Secret scanning and push protection are GitHub settings and stay as they are. |
| The teammate uses their own accounts | Written here. Access stays as it is until the owner sends an invite. |
| Repository settings stay unchanged | Written here. Access, branch protection, billing, and deployments stay as they are. |

This repository is public. GitHub documents that branch protection rules are available on public repositories for GitHub Free: [About protected branches](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches). The owner confirms the current plan on the repository settings page. Enabling a rule is a manual settings change. This repository's files leave protection untouched.

The settings UI may show Branches, Rules, or both. Rulesets are described in [About rulesets](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/about-rulesets).

When the plan shows branch protection for `main`, useful choices are: require a pull request before merging, require one approval, and require the status check `Check workflow scaffolding` after GitHub has recorded that name. Leave force-push and branch deletion disabled. Classic branch protection skips repository administrators unless the rule includes them. Read the settings page before relying on a rule to bind the owner.

The status check name is the job name in [`.github/workflows/workflow-check.yml`](../.github/workflows/workflow-check.yml).

## Onboarding

### Cursor

1. Install Cursor from [the official download page](https://cursor.com/download) and sign in with your own Cursor account. This repository does not install Cursor or sign anyone in.
2. Clone the repository and open that folder. Cloud agents need a Cursor account admin to connect source control first: [Cloud Agents](https://cursor.com/docs/cloud-agent).
3. Cursor loads the root [`AGENTS.md`](../AGENTS.md) file as plain markdown. The rules documentation is [Rules](https://cursor.com/docs/rules).
4. The agent follows `docs/COLLABORATION.md`. The operating rules in `AGENTS.md` are in context for Cursor. Claude Code's import syntax is documented for `CLAUDE.md`, and this repository uses it there.

### Claude Code

1. Install and sign in on your own machine. Use [Setup](https://code.claude.com/docs/en/setup), [Quickstart](https://code.claude.com/docs/en/quickstart), and [Overview](https://code.claude.com/docs/en/overview). This repository does not run the installer or complete a login.
2. In the repository directory, start `claude` and sign in with your own Claude account when prompted.
3. Claude Code loads [`CLAUDE.md`](../CLAUDE.md). The import line in that file uses Claude Code's documented `@path` syntax, outside backticks and code fences, to include this guide. Memory behavior is documented in [How Claude remembers your project](https://code.claude.com/docs/en/memory).
4. In the session, run `/context` and confirm `CLAUDE.md` appears under memory files. With both instruction files present, Claude Code's documented default is to load `CLAUDE.md`. The import is what places this guide in that session.
5. Paste the task the human provided.

Issue and pull request template behavior is documented in [About issue and pull request templates](https://docs.github.com/en/communities/using-templates-to-encourage-useful-issues-and-pull-requests/about-issue-and-pull-request-templates). GitHub uses the copies on `main`.

## Instruction files

- [`AGENTS.md`](../AGENTS.md) for Cursor and Grok Bot.
- [`CLAUDE.md`](../CLAUDE.md) for Claude Code.
- [`.github/ISSUE_TEMPLATE/task.md`](../.github/ISSUE_TEMPLATE/task.md) for a task.
- [`.github/PULL_REQUEST_TEMPLATE.md`](../.github/PULL_REQUEST_TEMPLATE.md) for a pull request.
- [`scripts/check_workflow.py`](../scripts/check_workflow.py) checks these files, local links, the Claude Code import, and the workflow syntax this repo can parse without added packages.
- [`.github/workflows/workflow-check.yml`](../.github/workflows/workflow-check.yml) runs that script with Python 3 already on the GitHub-hosted runner and GitHub's `actions/checkout` action at major version 4. The workflow installs nothing and reads no secrets.

After changing any of these files, run this from the repository root:

```bash
python3 scripts/check_workflow.py
```

Templates and the workflow on `main` are what day-to-day GitHub uses. They apply to new issues and pull requests after a human merges them to `main`.
