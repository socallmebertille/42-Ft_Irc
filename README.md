<div align="center" class="text-center">
  <h1>42-IRC</h1>

  <p><em>Internet Relay Chat</em></p>
  <img alt="last-commit" src="https://img.shields.io/github/last-commit/socallmebertille/42-IRC?style=flat&amp;logo=git&amp;logoColor=white&amp;color=0080ff" class="inline-block mx-1" style="margin: 0px 2px;">
  <img alt="repo-top-language" src="https://img.shields.io/github/languages/top/socallmebertille/42-IRC?style=flat&amp;color=0080ff" class="inline-block mx-1" style="margin: 0px 2px;">
  <img alt="repo-language-count" src="https://img.shields.io/github/languages/count/socallmebertille/42-IRC?style=flat&amp;color=0080ff" class="inline-block mx-1" style="margin: 0px 2px;">
  <p><em>Built with the tools and technologies:</em></p>
  <img alt="Markdown" src="https://img.shields.io/badge/Markdown-000000.svg?style=flat&amp;logo=Markdown&amp;logoColor=white" class="inline-block mx-1" style="margin: 0px 2px;">
  <img alt="GNU%20Bash" src="https://img.shields.io/badge/GNU%20Bash-4EAA25.svg?style=flat&amp;logo=GNU-Bash&amp;logoColor=white" class="inline-block mx-1" style="margin: 0px 2px;">
  <img alt="Docker" src="https://img.shields.io/badge/%20c++%20-00599C" class="inline-block mx-1" style="margin: 0px 2px;">
</div>

<h2>Table of Contents</h2>
<ul class="list-disc pl-4 my-0">
  <li class="my-0"><a href="#overview">Overview</a></li>
  <li class="my-0"><a href="#getting-started">Getting started</a>
  <ul class="list-disc pl-4 my-0">
    <li class="my-0"><a href="#step-1">Step 1</a></li>
  </ul>
  </li>
</ul>

## Overview

## Getting started

### Step 1
### utilisation issri
1. Lancer ton serveur IRC
./ircserv 6667 password

2. Ouvrir deux terminaux

Tu vas simuler deux clients :

    Un terminal pour Client A

    Un terminal pour Client B

3. Lancer irssi

Dans chaque terminal, tape :
irssi

4. Connexion au serveur

Dans irssi, tape cette commande :
/connect 127.0.0.1 6667 password

6667 : le port de ton serveur

127.0.0.1 : localhost

password : mot de passe attendu par le serveur

5. Changer de pseudo

Par exemple :
/nick mel
Dans le deuxième terminal :

/nick ugo


6. S’enregistrer

Tu peux t’enregistrer avec la commande :
/user melina 0 * :Melina Motylewski

7. Rejoindre un channel

Les deux clients tapent :
/join #test


8. Tester PART

Depuis mel, tape :
/part #test

9. Observer le résultat

Dans ugo, tu dois voir :
:mel!melina@localhost PART #test

🔁 Astuce pour revenir dans irssi

    Pour changer de fenêtre (ex : canal, status), fais Alt + 1, Alt + 2, etc.

    Pour quitter irssi proprement :
	/quit

📌 Résumé des commandes utiles
| Commande         | Description             |
| ---------------- | ----------------------- |
| `/server`        | Se connecter au serveur |
| `/nick`          | Changer de pseudo       |
| `/user`          | S’enregistrer           |
| `/join #channel` | Rejoindre un channel    |
| `/part #channel` | Quitter un channel      |
| `/quit`          | Quitter irssi           |


| Action dans irssi         | Commande envoyée au serveur |
| ------------------------- | --------------------------- |
| `/connect 127.0.0.1 6667` | (ouverture de socket TCP)   |
| `/nick mel`               | `NICK mel\r\n`              |
| `/user mel 0 * :Mel`      | `USER mel 0 * :Mel\r\n`     |
| `/join #42`               | `JOIN #42\r\n`              |
| `/part #42`               | `PART #42\r\n`              |
| `/msg #42 hello`          | `PRIVMSG #42 :hello\r\n`    |
| `/quit`                   | `QUIT :Client exited\r\n`   |
| `/whois mel`              | `WHOIS mel\r\n`             |
| etc.                      |                             |


# ✅ Tests fonctionnels du serveur IRC avec Irssi

Ce fichier regroupe tous les tests à effectuer avec le client IRC **irssi**, afin de vérifier que le serveur est conforme au sujet **ft_irc** de l'école 42.

---

## 🛠 Préparation

### 1. Lancer le serveur
```bash
./ircserv 6667 password
```

### 2. Lancer deux instances d’irssi dans deux terminaux
```bash
irssi
```

---

## 🧪 Test d’enregistrement d’un client

### Client 1 :
```irc
/connect 127.0.0.1 6667
/quote PASS password
/nick mel
/user mel 0 * :Melina Motylewski
```

### Client 2 (dans un autre terminal) :
```irc
/connect 127.0.0.1 6667
/quote PASS password
/nick ugo
/user ugo 0 * :Ugo Le Testeur
```

✅ Attendu :
- Le message `001 mel :Welcome to the IRC server!` est reçu
- Aucune erreur "required" ni "incorrect"
- Le pseudo est bien pris en compte

---

## 🔁 Gestion des collisions

### Dans un troisième terminal (ou depuis client 2 déjà connecté) :
```irc
/nick mel
```

❌ Attendu :
```
433 * mel :Nickname is already in use
```

---

## 📥 JOIN & Broadcast

### Client 1 :
```irc
/join #42
```

### Client 2 :
```irc
/join #42
```

✅ Attendu :
- Client 1 voit `ugo` rejoindre
- Client 2 voit `mel` déjà présent
- Pas d’erreur

---

## 💬 PRIVMSG

### Client 1 :
```irc
/msg #42 Hello à tous !
```

✅ Client 2 reçoit :
```
<mel> Hello à tous !
```

---

## 👋 PART

### Client 2 :
```irc
/part #42
```

✅ Client 1 voit :
```
:ugo!ugo@localhost PART #42
```

---

## 📨 NOTICE

### Client 1 :
```irc
/notice #42 Coucou notice
```

✅ Client 2 reçoit le message (sans retour d'erreur s'il n'existe pas)

---

## 🔗 QUIT

### Client 1 :
```irc
/quit :à bientôt
```

✅ Client 2 voit :
```
:mel!mel@localhost QUIT :à bientôt
```

---

## 🛰 PING/PONG

Envoyer un ping manuel :
```irc
/quote PING :12345
```

✅ Réponse :
```
PONG :12345
```

---

## 🔒 Mauvais mot de passe

```irc
/quote PASS nope
```

❌ Réponse :
```
464 :Password incorrect
```

---

## 🧼 Commande inconnue

```irc
/quote FOOBAR
```

❌ Réponse :
```
421 FOOBAR :Unknown command
```

---

## 🧾 Codes de réponse à gérer

| Code | Signification |
|------|----------------|
| 001  | Welcome |
| 433  | Nickname in use |
| 451  | Not registered |
| 464  | Password required/incorrect |
| 461  | Not enough parameters |
| 403  | No such channel |
| 401  | No such nick |
| 421  | Unknown command |

---

## ✅ Conclusion

Si tous les tests ci-dessus passent dans `irssi` **sans `/quote` sauf pour `PASS`**, alors votre serveur est **conforme au sujet** ft_irc 👏









# ✅ Tests fonctionnels du serveur IRC avec Irssi

Ce fichier regroupe tous les tests à effectuer avec le client IRC **irssi**, afin de vérifier que le serveur est conforme au sujet **ft_irc** de l'école 42.

---

## 🛠 Préparation

### 1. Lancer le serveur
```bash
./ircserv 6667 password
```

### 2. Lancer deux instances d’irssi dans deux terminaux
```bash
irssi
```

---

## 🧪 Test d’enregistrement d’un client

### Client 1 :
```irc
/connect 127.0.0.1 6667
/quote PASS password
/nick mel
/user mel 0 * :Melina Motylewski
```

### Client 2 (dans un autre terminal) :
```irc
/connect 127.0.0.1 6667
/quote PASS password
/nick ugo
/user ugo 0 * :Ugo Le Testeur
```

✅ Attendu :
- Le message `001 mel :Welcome to the IRC server!` est reçu
- Aucune erreur "required" ni "incorrect"
- Le pseudo est bien pris en compte

---

## 🔁 Gestion des collisions

### Dans un troisième terminal (ou depuis client 2 déjà connecté) :
```irc
/nick mel
```

❌ Attendu :
```
433 * mel :Nickname is already in use
```

---

## 📥 JOIN & Broadcast

### Client 1 :
```irc
/join #42
```

### Client 2 :
```irc
/join #42
```

✅ Attendu :
- Client 1 voit `ugo` rejoindre
- Client 2 voit `mel` déjà présent
- Pas d’erreur

---

## 💬 PRIVMSG

### Client 1 :
```irc
/msg #42 Hello à tous !
```

✅ Client 2 reçoit :
```
<mel> Hello à tous !
```

---

## 👋 PART

### Client 2 :
```irc
/part #42
```

✅ Client 1 voit :
```
:ugo!ugo@localhost PART #42
```

---

## 📨 NOTICE

### Client 1 :
```irc
/notice #42 Coucou notice
```

✅ Client 2 reçoit le message (sans retour d'erreur s'il n'existe pas)

---

## 🔗 QUIT

### Client 1 :
```irc
/quit :à bientôt
```

✅ Client 2 voit :
```
:mel!mel@localhost QUIT :à bientôt
```

---

## 🛰 PING/PONG

Envoyer un ping manuel :
```irc
/quote PING :12345
```

✅ Réponse :
```
PONG :12345
```

---

## 🔒 Mauvais mot de passe

```irc
/quote PASS nope
```

❌ Réponse :
```
464 :Password incorrect
```

---

## 🧼 Commande inconnue

```irc
/quote FOOBAR
```

❌ Réponse :
```
421 FOOBAR :Unknown command
```

---

## 🧾 Codes de réponse à gérer

| Code | Signification |
|------|----------------|
| 001  | Welcome |
| 433  | Nickname in use |
| 451  | Not registered |
| 464  | Password required/incorrect |
| 461  | Not enough parameters |
| 403  | No such channel |
| 401  | No such nick |
| 421  | Unknown command |

---

## ✅ Conclusion

Si tous les tests ci-dessus passent dans `irssi` **sans `/quote` sauf pour `PASS`**, alors votre serveur est **conforme au sujet** ft_irc 👏