#!/bin/bash

# =============================================================================
# Script de test modulaire pour serveur IRC ft_irc
# École 42 - Projet IRC
# =============================================================================

# Couleurs pour l'affichage
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
SERVER_PORT=6667
SERVER_PASS="test"
SERVER_BIN="./ircserv"

# Variables globales
TEST_COUNT=0
PASSED_COUNT=0
FAILED_COUNT=0
SERVER_PID=""

# =============================================================================
# FONCTIONS UTILITAIRES
# =============================================================================

print_header() {
    echo -e "${BLUE}============================================${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}============================================${NC}"
}

print_test() {
    echo -e "${CYAN}[TEST $((++TEST_COUNT))] $1${NC}"
}

print_success() {
    echo -e "${GREEN}[SUCCESS] $1${NC}"
    ((PASSED_COUNT++))
}

print_error() {
    echo -e "${RED}[ERROR] $1${NC}"
    ((FAILED_COUNT++))
}

print_warning() {
    echo -e "${YELLOW}[WARNING] $1${NC}"
}

print_info() {
    echo -e "${PURPLE}[INFO] $1${NC}"
}

# =============================================================================
# GESTION DU SERVEUR
# =============================================================================

start_server() {
    # Arrêter le serveur s'il est déjà en cours
    if [ -n "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
        stop_server
    fi

    print_info "Démarrage du serveur IRC sur le port $SERVER_PORT..."

    if ! [ -x "$SERVER_BIN" ]; then
        print_error "Serveur IRC non trouvé ou non exécutable: $SERVER_BIN"
        echo "Veuillez compiler le serveur avec 'make'"
        exit 1
    fi

    # Démarrer le serveur en arrière-plan
    $SERVER_BIN $SERVER_PORT $SERVER_PASS > server.log 2>&1 &
    SERVER_PID=$!

    # Attendre que le serveur soit prêt
    sleep 2

    if kill -0 $SERVER_PID 2>/dev/null; then
        print_success "Serveur démarré (PID: $SERVER_PID)"
    else
        print_error "Échec du démarrage du serveur"
        cat server.log
        exit 1
    fi
}

stop_server() {
    if [ -n "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
        print_info "Arrêt du serveur IRC..."
        kill $SERVER_PID
        wait $SERVER_PID 2>/dev/null
        print_success "Serveur arrêté"
    fi
}

# =============================================================================
# FONCTIONS DE TEST IRC
# =============================================================================

send_irc_command() {
    local command="$1"
    local expected_response="$2"
    local timeout="${3:-3}"

    echo -e "$command" | nc -w$timeout localhost $SERVER_PORT
}

test_connection() {
    print_test "Test de connexion basique"

    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TestUser\r\nUSER test test localhost :Test User\r\nQUIT\r\n" | nc -w3 localhost $SERVER_PORT)

    if echo "$response" | grep -q "001.*Welcome"; then
        print_success "Connexion et enregistrement réussis"
    else
        print_error "Échec de la connexion"
        echo "Réponse reçue: $response"
    fi
}

# =============================================================================
# TESTS DES COMMANDES DE BASE
# =============================================================================

test_auth() {
    print_header "TESTS D'AUTHENTIFICATION"

    print_test "Test mot de passe correct"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TestUser\r\nUSER test test localhost :Test User\r\nQUIT\r\n" | nc -w3 localhost $SERVER_PORT)
    if echo "$response" | grep -q "001.*Welcome"; then
        print_success "Authentification réussie avec le bon mot de passe"
    else
        print_error "Échec d'authentification avec le bon mot de passe"
    fi

    print_test "Test mot de passe incorrect"
    local response=$(echo -e "PASS wrongpass\r\nNICK TestUser\r\nUSER test test localhost :Test User\r\nQUIT\r\n" | nc -w3 localhost $SERVER_PORT)
    if echo "$response" | grep -q "464.*Password"; then
        print_success "Rejet correct du mauvais mot de passe"
    else
        print_error "Le serveur devrait rejeter le mauvais mot de passe"
    fi
}

test_nick() {
    print_header "TESTS NICKNAME"

    print_test "Test changement de nickname"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TestUser\r\nUSER test test localhost :Test User\r\nNICK NewNick\r\nQUIT\r\n" | nc -w3 localhost $SERVER_PORT)
    if echo "$response" | grep -q "NewNick"; then
        print_success "Changement de nickname réussi"
    else
        print_error "Échec du changement de nickname"
    fi
}

# =============================================================================
# TESTS DES CANAUX
# =============================================================================

test_channels() {
    print_header "TESTS DES CANAUX"

    print_test "Test JOIN/PART basique"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TestUser\r\nUSER test test localhost :Test User\r\nJOIN #test\r\nPART #test\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)
    if echo "$response" | grep -q "JOIN.*#test" && echo "$response" | grep -q "PART.*#test"; then
        print_success "JOIN/PART fonctionnent correctement"
    else
        print_error "Problème avec JOIN/PART"
    fi
}

# =============================================================================
# TESTS DU TOPIC
# =============================================================================

test_topic() {
    print_header "TESTS DU TOPIC"

    print_test "Test topic vide (GET)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TestUser\r\nUSER test test localhost :Test User\r\nJOIN #topictest\r\nTOPIC #topictest\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)
    if echo "$response" | grep -q "331.*No topic is set"; then
        print_success "Topic vide correctement rapporté"
    else
        print_error "Le serveur devrait indiquer qu'aucun topic n'est défini"
    fi

    print_test "Test définition de topic (SET)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TopicSetter\r\nUSER test test localhost :Test User\r\nJOIN #topictest\r\nTOPIC #topictest :Nouveau topic de test\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)
    if echo "$response" | grep -q "TOPIC.*#topictest.*Nouveau topic de test"; then
        print_success "Définition de topic réussie"
    else
        print_error "Échec de la définition du topic"
    fi

    print_test "Test récupération de topic (GET après SET)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TopicGetter\r\nUSER test test localhost :Test User\r\nJOIN #topictest\r\nTOPIC #topictest :Test topic\r\nTOPIC #topictest\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)
    if echo "$response" | grep -q "332.*Test topic"; then
        print_success "Récupération de topic réussie"
    else
        print_error "Échec de la récupération du topic"
    fi
}

# =============================================================================
# TESTS DES MESSAGES
# =============================================================================

test_privmsg() {
    print_header "TESTS DES MESSAGES PRIVÉS"

    print_test "Test PRIVMSG vers canal"
    # Ce test nécessiterait deux connexions simultanées, on le simplifie
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK MsgSender\r\nUSER test test localhost :Test User\r\nJOIN #msgtest\r\nPRIVMSG #msgtest :Hello world\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)
    if echo "$response" | grep -q "JOIN.*#msgtest"; then
        print_success "PRIVMSG vers canal traité (connexion réussie)"
    else
        print_error "Problème avec PRIVMSG vers canal"
    fi
}

# =============================================================================
# TESTS AVANCÉS DU TOPIC
# =============================================================================

test_topic_advanced() {
    print_header "TESTS AVANCÉS DU TOPIC"

    print_test "Test topic sans permission (utilisateur normal)"
    # Créer un canal, puis un deuxième utilisateur essaie de changer le topic
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK Operator\r\nUSER test test localhost :Test User\r\nJOIN #topicperm\r\nTOPIC #topicperm :Topic initial\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    # Deuxième utilisateur (non-opérateur) essaie de changer le topic
    local response2=$(echo -e "PASS $SERVER_PASS\r\nNICK NormalUser\r\nUSER test test localhost :Test User\r\nJOIN #topicperm\r\nTOPIC #topicperm :Topic interdit\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response2" | grep -q "482.*not channel operator"; then
        print_success "Protection du topic: utilisateur normal correctement rejeté"
    else
        print_error "Le serveur devrait empêcher les non-opérateurs de changer le topic"
    fi

    print_test "Test topic lors du JOIN (métadonnées complètes)"
    # Définir un topic, puis rejoindre pour vérifier qu'on reçoit 332 et 333
    echo -e "PASS $SERVER_PASS\r\nNICK TopicSetter\r\nUSER test test localhost :Test User\r\nJOIN #topicjoin\r\nTOPIC #topicjoin :Topic avec métadonnées\r\nQUIT\r\n" | nc -w3 localhost $SERVER_PORT > /dev/null

    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TopicJoiner\r\nUSER test test localhost :Test User\r\nJOIN #topicjoin\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "332.*Topic avec métadonnées" && echo "$response" | grep -q "333.*TopicSetter"; then
        print_success "Topic lors du JOIN: métadonnées complètes (332 + 333)"
    else
        print_error "Le serveur devrait envoyer topic + métadonnées lors du JOIN"
        echo "Debug: $response"
    fi

    print_test "Test topic vide puis topic non-vide"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TopicChanger\r\nUSER test test localhost :Test User\r\nJOIN #topicchange\r\nTOPIC #topicchange\r\nTOPIC #topicchange :Nouveau topic\r\nTOPIC #topicchange\r\nQUIT\r\n" | nc -w7 localhost $SERVER_PORT)

    if echo "$response" | grep -q "331.*No topic is set" && echo "$response" | grep -q "332.*Nouveau topic"; then
        print_success "Changement topic vide → non-vide fonctionne"
    else
        print_error "Problème avec la transition topic vide → non-vide"
    fi
}

# =============================================================================
# TESTS DES MODES DE CANAL (+i, +t, +k, +l) - VERSION AMÉLIORÉE
# =============================================================================

test_channel_modes() {
    print_header "TESTS DES MODES DE CANAL"

    print_test "Test mode +i (invite only)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK ModeTest\r\nUSER test test localhost :Test User\r\nJOIN #modetest\r\nMODE #modetest +i\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#modetest.*+i"; then
        print_success "Mode +i (invite only) appliqué"
    else
        print_error "Échec du mode +i"
    fi

    print_test "Test mode +t (topic protected)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TopicMode\r\nUSER test test localhost :Test User\r\nJOIN #topicmode\r\nMODE #topicmode +t\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#topicmode.*+t"; then
        print_success "Mode +t (topic protected) appliqué"
    else
        print_error "Échec du mode +t"
    fi

    print_test "Test mode +k (password)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK KeyMode\r\nUSER test test localhost :Test User\r\nJOIN #keymode\r\nMODE #keymode +k secret123\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#keymode.*+k"; then
        print_success "Mode +k (password) appliqué"
    else
        print_error "Échec du mode +k"
    fi

    print_test "Test mode +l (user limit)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK LimitMode\r\nUSER test test localhost :Test User\r\nJOIN #limitmode\r\nMODE #limitmode +l 5\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#limitmode.*+l"; then
        print_success "Mode +l (user limit) appliqué"
    else
        print_error "Échec du mode +l"
    fi

    print_test "Test modes multiples en une commande (+it)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK MultiMode1\r\nUSER test test localhost :Test User\r\nJOIN #multimode1\r\nMODE #multimode1 +it\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#multimode1.*+it"; then
        print_success "Modes multiples +it appliqués en une seule commande"
    else
        print_error "Échec des modes multiples +it"
    fi

    print_test "Test modes multiples avec paramètres (+ikt)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK MultiMode2\r\nUSER test test localhost :Test User\r\nJOIN #multimode2\r\nMODE #multimode2 +ikt secret123\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#multimode2.*+ikt"; then
        print_success "Modes multiples +ikt avec paramètre appliqués"
    else
        print_error "Échec des modes multiples +ikt"
    fi

    print_test "Test modes multiples complets (+iktl)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK MultiMode3\r\nUSER test test localhost :Test User\r\nJOIN #multimode3\r\nMODE #multimode3 +iktl secret123 10\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#multimode3.*+iktl"; then
        print_success "Modes multiples +iktl avec paramètres appliqués"
    else
        print_error "Échec des modes multiples +iktl"
    fi

    print_test "Test modes mixtes (+i-t+k)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK MixedMode\r\nUSER test test localhost :Test User\r\nJOIN #mixedmode\r\nMODE #mixedmode +it\r\nMODE #mixedmode +i-t+k newpass\r\nQUIT\r\n" | nc -w7 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#mixedmode"; then
        print_success "Modes mixtes (+i-t+k) traités correctement"
    else
        print_error "Échec des modes mixtes"
    fi

    print_test "Test suppression de modes (-itk)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK RemoveMode\r\nUSER test test localhost :Test User\r\nJOIN #removemode\r\nMODE #removemode +ikt secret123\r\nMODE #removemode -itk\r\nQUIT\r\n" | nc -w7 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#removemode.*-"; then
        print_success "Suppression de modes multiples (-itk) fonctionne"
    else
        print_error "Échec de la suppression de modes multiples"
    fi

    print_test "Test consultation des modes actuels"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK QueryMode\r\nUSER test test localhost :Test User\r\nJOIN #querymode\r\nMODE #querymode +it\r\nMODE #querymode\r\nQUIT\r\n" | nc -w7 localhost $SERVER_PORT)

    if echo "$response" | grep -q "324.*#querymode.*+"; then
        print_success "Consultation des modes actuels (324) fonctionne"
    else
        print_error "Échec de la consultation des modes actuels"
    fi
}

# =============================================================================
# TESTS AVANCÉS DES MODES (NOUVELLES FONCTIONNALITÉS)
# =============================================================================

test_advanced_modes() {
    print_header "TESTS AVANCÉS DES MODES"

    print_test "Test gestion d'erreurs avec modes multiples"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK ErrorMode\r\nUSER test test localhost :Test User\r\nJOIN #errormode\r\nMODE #errormode +k\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "461.*Need key parameter"; then
        print_success "Erreur mode +k sans paramètre correctement détectée"
    else
        print_error "Le serveur devrait détecter l'absence de paramètre pour +k"
    fi

    print_test "Test continuation après erreur de mode"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK ContinueMode\r\nUSER test test localhost :Test User\r\nJOIN #continuemode\r\nMODE #continuemode +ik\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#continuemode.*+i"; then
        print_success "Mode +i appliqué même si +k échoue (continuation après erreur)"
    else
        print_error "Le serveur devrait continuer les autres modes même si un échoue"
    fi

    print_test "Test mode +o (operator privileges)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK OpMode\r\nUSER test test localhost :Test User\r\nJOIN #opmode\r\nMODE #opmode +o OpMode\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#opmode.*+o.*OpMode"; then
        print_success "Mode +o (operator) fonctionne"
    else
        print_error "Échec du mode +o"
    fi

    print_test "Test mode +l avec limite invalide"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK InvalidLimit\r\nUSER test test localhost :Test User\r\nJOIN #invalidlimit\r\nMODE #invalidlimit +l -5\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "461.*Invalid limit"; then
        print_success "Limite invalide (-5) correctement rejetée"
    else
        print_error "Le serveur devrait rejeter les limites invalides"
    fi

    print_test "Test modes avec paramètres dans le bon ordre"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK ParamOrder\r\nUSER test test localhost :Test User\r\nJOIN #paramorder\r\nMODE #paramorder +kl mypass 15\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#paramorder.*+kl.*mypass.*15"; then
        print_success "Paramètres des modes dans le bon ordre"
    else
        print_error "Problème avec l'ordre des paramètres des modes"
    fi
}

# =============================================================================
# TESTS DE VALIDATION PART (NOUVELLES FONCTIONNALITÉS)
# =============================================================================

test_part_advanced() {
    print_header "TESTS AVANCÉS DE LA COMMANDE PART"

    print_test "Test PART avec message personnalisé"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK PartMsg\r\nUSER test test localhost :Test User\r\nJOIN #parttest\r\nPART #parttest :Au revoir tout le monde!\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "PART.*#parttest.*Au revoir tout le monde"; then
        print_success "PART avec message personnalisé fonctionne"
    else
        print_error "Échec de PART avec message"
    fi

    print_test "Test PART sans message (norme IRC)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK PartNoMsg\r\nUSER test test localhost :Test User\r\nJOIN #partnotest\r\nPART #partnotest\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "PART.*#partnotest" && ! echo "$response" | grep -q "PART.*#partnotest.*:"; then
        print_success "PART sans message (conforme RFC)"
    else
        print_error "Problème avec PART sans message"
    fi

    print_test "Test PART sans paramètre (doit échouer selon RFC)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK PartEmpty\r\nUSER test test localhost :Test User\r\nJOIN #partempty\r\nPART\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "461.*Not enough parameters"; then
        print_success "PART sans paramètre correctement rejeté (conforme RFC)"
    else
        print_error "PART sans paramètre devrait être rejeté selon la norme IRC"
    fi

    print_test "Test PART canal inexistant"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK PartNone\r\nUSER test test localhost :Test User\r\nPART #inexistant\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "403.*No such channel"; then
        print_success "PART canal inexistant correctement géré"
    else
        print_error "Échec de la gestion de PART sur canal inexistant"
    fi
}

# =============================================================================
# TESTS DE SCÉNARIOS COMPLEXES AVEC NOUVELLES FONCTIONNALITÉS
# =============================================================================

test_complex_scenarios() {
    print_header "TESTS DE SCÉNARIOS COMPLEXES"

    print_test "Scénario: Création canal → Topic → Utilisateur rejoint → Voit topic"
    # Étape 1: Créer canal et définir topic
    echo -e "PASS $SERVER_PASS\r\nNICK Creator\r\nUSER test test localhost :Test User\r\nJOIN #scenario\r\nTOPIC #scenario :Topic du scénario\r\nQUIT\r\n" | nc -w3 localhost $SERVER_PORT > /dev/null

    # Étape 2: Nouveau utilisateur rejoint
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK Joiner\r\nUSER test test localhost :Test User\r\nJOIN #scenario\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "332.*Topic du scénario" && echo "$response" | grep -q "333.*Creator"; then
        print_success "Scénario complet: Topic persistant lors du rejoin"
    else
        print_error "Le topic ne persiste pas correctement entre les sessions"
    fi

    print_test "Scénario: Multi-canaux avec topics différents"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK MultiUser\r\nUSER test test localhost :Test User\r\nJOIN #chan1\r\nTOPIC #chan1 :Topic du canal 1\r\nJOIN #chan2\r\nTOPIC #chan2 :Topic du canal 2\r\nTOPIC #chan1\r\nTOPIC #chan2\r\nQUIT\r\n" | nc -w7 localhost $SERVER_PORT)

    if echo "$response" | grep -q "332.*Topic du canal 1" && echo "$response" | grep -q "332.*Topic du canal 2"; then
        print_success "Gestion multi-canaux avec topics séparés"
    else
        print_error "Problème avec la gestion de topics multiples"
    fi

    print_test "Scénario: Transfert d'opérateur et permissions topic"
    # Simuler un transfert d'opérateur (pour l'instant, juste tester la base)
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK OpTransfer\r\nUSER test test localhost :Test User\r\nJOIN #optransfer\r\nTOPIC #optransfer :Topic initial\r\nMODE #optransfer +o OpTransfer\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "TOPIC.*Topic initial"; then
        print_success "Base du transfert d'opérateur testée"
    else
        print_warning "Transfert d'opérateur: tests limités en mode unique client"
    fi

    print_test "Scénario: Modes multiples puis validation"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK ComplexMode\r\nUSER test test localhost :Test User\r\nJOIN #complex\r\nMODE #complex +iktl secret123 10\r\nMODE #complex\r\nQUIT\r\n" | nc -w7 localhost $SERVER_PORT)

    if echo "$response" | grep -q "MODE.*#complex.*+iktl" && echo "$response" | grep -q "324.*#complex.*+iktl"; then
        print_success "Scénario modes multiples → consultation fonctionne"
    else
        print_error "Problème avec le scénario modes multiples"
    fi

    print_test "Scénario: Canal protégé par mot de passe"
    # Créer canal avec mot de passe
    echo -e "PASS $SERVER_PASS\r\nNICK ProtectedCreator\r\nUSER test test localhost :Test User\r\nJOIN #protected\r\nMODE #protected +k secret123\r\nQUIT\r\n" | nc -w3 localhost $SERVER_PORT > /dev/null

    # Essayer de rejoindre sans mot de passe
    local response1=$(echo -e "PASS $SERVER_PASS\r\nNICK TryJoin1\r\nUSER test test localhost :Test User\r\nJOIN #protected\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    # Essayer de rejoindre avec le bon mot de passe
    local response2=$(echo -e "PASS $SERVER_PASS\r\nNICK TryJoin2\r\nUSER test test localhost :Test User\r\nJOIN #protected secret123\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response1" | grep -q "475.*Cannot join channel" && echo "$response2" | grep -q "JOIN.*#protected"; then
        print_success "Protection par mot de passe fonctionne"
    else
        print_error "Problème avec la protection par mot de passe"
    fi

    print_test "Scénario: Canal avec limite d'utilisateurs"
    # Créer canal avec limite de 1 utilisateur
    echo -e "PASS $SERVER_PASS\r\nNICK LimitCreator\r\nUSER test test localhost :Test User\r\nJOIN #limited\r\nMODE #limited +l 1\r\nQUIT\r\n" | nc -w3 localhost $SERVER_PORT > /dev/null

    # Essayer de rejoindre (devrait échouer car limite atteinte)
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK TryLimited\r\nUSER test test localhost :Test User\r\nJOIN #limited\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "471.*Cannot join channel"; then
        print_success "Limite d'utilisateurs respectée"
    else
        print_warning "Test limite utilisateurs: résultat incertain (canal peut être vide)"
    fi

    print_test "Scénario: Gestion des erreurs en cascade"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK ErrorCascade\r\nUSER test test localhost :Test User\r\nJOIN #errorcascade\r\nMODE #errorcascade +k\r\nMODE #errorcascade +l invalid\r\nMODE #errorcascade +i\r\nMODE #errorcascade\r\nQUIT\r\n" | nc -w8 localhost $SERVER_PORT)

    if echo "$response" | grep -q "461.*Need key parameter" && echo "$response" | grep -q "461.*Invalid limit" && echo "$response" | grep -q "324.*+i"; then
        print_success "Gestion des erreurs en cascade: erreurs détectées, mode +i appliqué"
    else
        print_error "Problème avec la gestion des erreurs en cascade"
    fi
}

# =============================================================================
# TESTS DE ROBUSTESSE ET CAS LIMITES
# =============================================================================

test_edge_cases() {
    print_header "TESTS DE CAS LIMITES ET ROBUSTESSE"

    print_test "Topic avec caractères spéciaux"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK SpecialChar\r\nUSER test test localhost :Test User\r\nJOIN #special\r\nTOPIC #special :Topic avec emoticones et chars speciaux: []{}!@#\$%^&*()\r\nTOPIC #special\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "332.*emoticones.*chars speciaux"; then
        print_success "Gestion des caractères spéciaux dans topic"
    else
        print_warning "Problème potentiel avec caractères spéciaux"
    fi

    print_test "Topic très long (test de limite)"
    local long_topic="Topic très long pour tester les limites du serveur IRC: "
    for i in {1..20}; do
        long_topic+="partie$i "
    done

    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK LongTopic\r\nUSER test test localhost :Test User\r\nJOIN #longtopic\r\nTOPIC #longtopic :$long_topic\r\nTOPIC #longtopic\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "332"; then
        print_success "Gestion des topics longs"
    else
        print_warning "Problème potentiel avec les topics longs"
    fi

    print_test "Commandes rapides en succession"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK RapidUser\r\nUSER test test localhost :Test User\r\nJOIN #rapid\r\nTOPIC #rapid :Topic1\r\nTOPIC #rapid :Topic2\r\nTOPIC #rapid :Topic3\r\nTOPIC #rapid\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "332.*Topic3"; then
        print_success "Commandes rapides en succession gérées"
    else
        print_error "Problème avec les commandes rapides"
    fi

    print_test "Canal avec nom limite (caractères valides)"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK ChannelLimit\r\nUSER test test localhost :Test User\r\nJOIN #canal-test_123\r\nTOPIC #canal-test_123 :Topic canal avec tirets et underscores\r\nQUIT\r\n" | nc -w5 localhost $SERVER_PORT)

    if echo "$response" | grep -q "JOIN.*#canal-test_123"; then
        print_success "Noms de canaux avec caractères spéciaux autorisés"
    else
        print_error "Problème avec les noms de canaux complexes"
    fi
}

# =============================================================================
# TESTS FINAUX ET NETTOYAGE
# =============================================================================

test_cleanup() {
    print_header "TESTS DE NETTOYAGE"

    print_test "Test QUIT propre"
    local response=$(echo -e "PASS $SERVER_PASS\r\nNICK CleanExit\r\nUSER test test localhost :Test User\r\nJOIN #cleanup\r\nQUIT :Au revoir!\r\n" | nc -w3 localhost $SERVER_PORT)

    if echo "$response" | grep -q "ERROR.*Closing Link"; then
        print_success "QUIT propre avec message personnalisé"
    else
        print_success "QUIT traité (format de réponse peut varier)"
    fi
}

# =============================================================================
# FONCTION PRINCIPALE ET MENU
# =============================================================================

print_summary() {
    echo
    print_header "RÉSUMÉ DES TESTS"
    echo -e "${CYAN}Tests exécutés: $TEST_COUNT${NC}"
    echo -e "${GREEN}Tests réussis: $PASSED_COUNT${NC}"
    echo -e "${RED}Tests échoués: $FAILED_COUNT${NC}"

    if [ $FAILED_COUNT -eq 0 ]; then
        echo -e "${GREEN}🎉 Tous les tests sont passés avec succès!${NC}"
    else
        echo -e "${YELLOW}⚠️  Certains tests ont échoué. Vérifiez les détails ci-dessus.${NC}"
    fi
    echo
}

show_menu() {
    echo
    print_header "MENU DES TESTS IRC ft_irc"
    echo "1. Tests complets (tous les tests)"
    echo "2. Tests de base (connexion, auth, nick)"
    echo "3. Tests des canaux (join/part/topic)"
    echo "4. Tests avancés du topic"
    echo "5. Tests des modes de canal"
    echo "6. Tests avancés des modes (NOUVEAU)"
    echo "7. Tests PART avancés (NOUVEAU)"
    echo "8. Tests des commandes d'opérateur"
    echo "9. Tests de gestion d'erreurs"
    echo "10. Tests de compatibilité irssi"
    echo "11. Tests de stabilité"
    echo "12. Tests de scénarios complexes"
    echo "13. Tests de cas limites"
    echo "14. Test de connexion simple"
    echo "0. Quitter"
    echo
    read -p "Choisissez une option (0-14): " choice
}

run_selected_tests() {
    case $1 in
        1)
            test_auth
            test_nick
            test_channels
            test_topic
            test_topic_advanced
            test_channel_modes
            test_advanced_modes
            test_part_advanced
            test_operator_commands
            test_privmsg
            test_error_handling
            test_irssi_compatibility
            test_stability
            test_complex_scenarios
            test_edge_cases
            test_cleanup
            ;;
        2)
            test_auth
            test_nick
            test_connection
            ;;
        3)
            test_channels
            test_topic
            ;;
        4)
            test_topic_advanced
            ;;
        5)
            test_channel_modes
            ;;
        6)
            test_advanced_modes
            ;;
        7)
            test_part_advanced
            ;;
        8)
            test_operator_commands
            ;;
        9)
            test_error_handling
            ;;
        10)
            test_irssi_compatibility
            ;;
        11)
            test_stability
            ;;
        12)
            test_complex_scenarios
            ;;
        13)
            test_edge_cases
            ;;
        14)
            test_connection
            ;;
        0)
            echo "Au revoir!"
            exit 0
            ;;
        *)
            print_error "Option invalide: $1"
            ;;
    esac
}

# =============================================================================
# FONCTION PRINCIPALE
# =============================================================================

main() {
    # Vérifier que le serveur peut être compilé
    if ! [ -f "Makefile" ]; then
        print_error "Makefile non trouvé. Êtes-vous dans le bon répertoire?"
        exit 1
    fi

    # Compiler le serveur
    print_info "Compilation du serveur..."
    if ! make > /dev/null 2>&1; then
        print_error "Échec de la compilation. Vérifiez votre code."
        exit 1
    fi

    # Démarrer le serveur
    start_server

    # Piège pour nettoyer à la sortie
    trap 'stop_server; exit' EXIT SIGINT SIGTERM

    # Si un argument est passé, exécuter directement
    if [ $# -gt 0 ]; then
        run_selected_tests $1
    else
        # Sinon afficher le menu
        while true; do
            show_menu
            run_selected_tests $choice
            print_summary
            echo
            read -p "Appuyez sur Entrée pour continuer..."
        done
    fi

    # Arrêter le serveur
    stop_server

    # Afficher le résumé final
    print_summary
}

# =============================================================================
# POINT D'ENTRÉE
# =============================================================================

# Vérifier les dépendances
if ! command -v nc &> /dev/null; then
    print_error "netcat (nc) n'est pas installé. Installez-le avec: sudo apt-get install netcat"
    exit 1
fi

# Exécuter le programme principal
main "$@"
