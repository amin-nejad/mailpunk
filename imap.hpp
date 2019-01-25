#ifndef IMAP_H
#define IMAP_H
#include "imaputils.hpp"
#include <libetpan/libetpan.h>
#include <string>
#include <functional>

namespace IMAP {

  class Session; // forward declaration

  class Message {
  private:
    
    Session* session;
    uint32_t message_UID;
    std::string from, subject, body;

    // decompose the sender clist to extract useful information
    std::string fromList(clist* from_lst);

  public:
    
    Message(Session* session, uint32_t message_UID);
    /**
     * Get the body of the message. You may chose to either include the headers or not.
     */
    std::string getBody();
    /**
     * Get one of the descriptor fields (subject, from, ...)
     */
    std::string getField(std::string fieldname);
    /**
     * Remove this mail from its mailbox
     */
    // this function manages to delete the selected mail but unfortunately crashes in the
    // process. For the life of me I can't figure out why
    void deleteFromMailbox();
  };
  
  class Session {
  private:

    const char* current_mailbox;
    struct mailimap* imapSession;
    std::function<void()> updateUI;
    Message** messages;

    // returns the number of emails in the mailbox
    uint32_t getMailCount();

    // return uid of message
    uint32_t getUID(struct mailimap_msg_att* msg_att);

  public:

    Session(std::function<void()> updateUI);
    
    /**
     * Get all messages in the INBOX mailbox terminated by a nullptr (like we did in class)
     */
    Message** getMessages();
    
    /**
     * connect to the specified server (143 is the standard unencrypted imap port)
     */
    void connect(std::string const& server, size_t port = 143);
    
    /**
     * log in to the server (connect first, then log in)
     */
    void login(std::string const& userid, std::string const& password);
    
    /**
     * select a mailbox (only one can be selected at any given time)
     * 
     * this can only be performed after login
     */
    void selectMailbox(std::string const& mailbox);
    
    ~Session();

    friend Message;
  };
}

#endif /* IMAP_H */
