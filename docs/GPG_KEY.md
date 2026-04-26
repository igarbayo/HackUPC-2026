# GPG Key Setup and Commit Signing

This document explains how to generate a GPG key, configure Git to sign
commits with it, verify signed commits, and publish your public key so that
others can verify your identity.

All XEITECH maintainers are expected to sign their commits. This ensures that
every commit in the repository can be traced to a verified author and protects
the project against supply-chain attacks where an attacker pushes commits
impersonating a maintainer.

---

## 1. Generate a GPG key

```bash
gpg --full-generate-key
```

When prompted:

- **Key type:** `RSA and RSA` (option 1) or `ECC (sign and encrypt)` for a
  more modern key.
- **Key size:** `4096` bits (for RSA) or `Curve 25519` (for ECC).
- **Expiry:** `2y` (two years) is a reasonable default. You can extend it later.
- **Name and email:** use your real name and the email address associated with
  your GitHub account.

Verify the key was created:

```bash
gpg --list-secret-keys --keyid-format LONG
```

The output looks like:

```
sec   rsa4096/ABCDEF1234567890 2026-04-26 [SC] [expires: 2028-04-26]
      FINGERPRINT...
uid   [ultimate] Your Name <your@email.com>
```

The part after `rsa4096/` (here `ABCDEF1234567890`) is your **Key ID**.

---

## 2. Configure Git to use the key

```bash
git config --global user.signingkey ABCDEF1234567890
git config --global commit.gpgsign true
```

Replace `ABCDEF1234567890` with your actual Key ID.

From this point on, every `git commit` will be signed automatically. If you
want to sign only specific commits:

```bash
git commit -S -m "feat: add feature"
```

---

## 3. Add your public key to GitHub

Export your public key:

```bash
gpg --armor --export ABCDEF1234567890
```

Copy the entire output (starting with `-----BEGIN PGP PUBLIC KEY BLOCK-----`).

On GitHub: **Settings → SSH and GPG keys → New GPG key** → paste the key.

GitHub will then display a green "Verified" badge next to your signed commits.

---

## 4. Publish your key to a public keyserver

Publishing allows anyone to verify your commits without asking you for the
key directly:

```bash
gpg --keyserver keys.openpgp.org --send-keys ABCDEF1234567890
```

You can also use `keyserver.ubuntu.com` or `pgp.mit.edu`.

---

## 5. Verify a signed commit

To inspect the signature on any commit:

```bash
git log --show-signature -1
```

A valid signature produces output that includes:

```
gpg: Signature made ...
gpg: Good signature from "Your Name <your@email.com>"
```

To verify every commit in a range:

```bash
git log --show-signature main..develop
```

---

## 6. Import another maintainer's key

To verify commits made by other maintainers, import their public key:

```bash
gpg --keyserver keys.openpgp.org --recv-keys <THEIR_KEY_ID>
```

Or ask them to send you the exported key directly and import it:

```bash
gpg --import their-key.asc
```

---

## 7. Extend an expired key

Keys expire for security hygiene. To extend the expiry date:

```bash
gpg --edit-key ABCDEF1234567890
# Inside the interactive prompt:
gpg> expire
# Follow the prompts to set a new expiry
gpg> save
```

Then re-upload the updated key to the keyserver and to GitHub.

---

## 8. Developer Certificate of Origin

In addition to GPG signing, all contributions must comply with the
Developer Certificate of Origin (DCO). By signing your commits you certify
that you have the right to submit the contribution under the project's license.

The full DCO text is in [DCO](../DCO) at the repository root.

To acknowledge the DCO, add a `Signed-off-by` line to your commit message
footer:

```
feat: add new feature

Signed-off-by: Your Name <your@email.com>
```

With Git, this can be added automatically using the `-s` flag:

```bash
git commit -s -m "feat: add new feature"
```
