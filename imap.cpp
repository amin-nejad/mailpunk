#include "imap.hpp"

using namespace std;

IMAP::Session::Session(std::function<void()> updateUI)
  :updateUI(updateUI){

  imapSession = mailimap_new(0, NULL);
}

void IMAP::Session::connect(string const& server, size_t port){

  check_error(mailimap_socket_connect(imapSession,
				      server.c_str(),
				      port),
              "ERROR! Unable to connect to server.");
}

void IMAP::Session::login(string const& userid, string const& password){

  check_error(mailimap_login(imapSession,
			     userid.c_str(),
			     password.c_str()),
              "ERROR! Unable to login. Please check username/password.");
}

void IMAP::Session::selectMailbox(std::string const& mailbox){

  current_mailbox = mailbox.c_str();
  check_error(mailimap_select(imapSession, mailbox.c_str()),
              "ERROR! Unable to select mailbox.");
}

IMAP::Message** IMAP::Session::getMessages(){
  
  auto set = mailimap_set_new_interval(1, 0);
  auto fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
  auto fetch_att = mailimap_fetch_att_new_uid();
  clistiter* cur;
  clist* fetch_result;
  uint32_t number_of_messages = getMailCount();
    
  check_error(mailimap_fetch_type_new_fetch_att_list_add(fetch_type, fetch_att),
	      "ERROR! Unable to add attributes to new fetch.");
  
  check_error(mailimap_fetch(imapSession, set, fetch_type, &fetch_result),
	      "ERROR! Unable to fetch messages.");

  messages = new Message*[number_of_messages + 1];
  
  for (cur = clist_begin(fetch_result); cur != NULL; cur = clist_next(cur)) {
    
    auto msg_att = (mailimap_msg_att*) clist_content(cur);
    uint32_t UID = getUID(msg_att);
    
    int i = 0;
    if (UID != 0) {

      for (; messages[i] != NULL; i++){

	messages[i] = new Message(this, UID);
      }
      
      messages[i+1] = nullptr;

    } else {

      messages[i] = nullptr;
    }
  }

  // free memory
  mailimap_fetch_list_free(fetch_result);
  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);
  
  return messages; 
}


uint32_t IMAP::Session::getUID(struct mailimap_msg_att* msg_att) {

  clistiter* cur;
    
  for (cur = clist_begin(msg_att->att_list); cur != NULL; cur = clist_next(cur)) {

    mailimap_msg_att_item* item = (mailimap_msg_att_item*) clist_content(cur);

    if (item->att_type == MAILIMAP_MSG_ATT_ITEM_STATIC &&
	item->att_data.att_static->att_type == MAILIMAP_MSG_ATT_UID){

      return item->att_data.att_static->att_data.att_uid;
    }
  }
  return 0;
}

uint32_t IMAP::Session::getMailCount() {

  uint32_t number_of_messages = 0;
  auto status_att_list = mailimap_status_att_list_new_empty();
  mailimap_mailbox_data_status* result;
    
  check_error(mailimap_status_att_list_add(status_att_list,
					   MAILIMAP_STATUS_ATT_MESSAGES),
	      "ERROR! Unable to add message status to attribute list");
  
  check_error(mailimap_status(imapSession, current_mailbox, status_att_list, &result),
	       "ERROR! Unable to retrieve mailbox status");
  
  for (clistiter* cur = clist_begin(result->st_info_list);
       cur != NULL ;
       cur = clist_next(cur)) {
    
    auto status_info = (mailimap_status_info*) clist_content(cur);

    if (status_info->st_att == MAILIMAP_STATUS_ATT_MESSAGES) {
      number_of_messages = status_info->st_value;
    }
  }

  // free memory
  mailimap_mailbox_data_status_free(result);
  mailimap_status_att_list_free(status_att_list);
  
  return number_of_messages;
}

IMAP::Session::~Session(){

  for (int i = 0; messages[i] != NULL; i++){
    delete messages[i];
  }

  mailimap_logout(imapSession);
  mailimap_free(imapSession);
}

/////////////////////////////////////////////////////////////////////////

IMAP::Message::Message(Session* session, uint32_t message_UID)
  :session(session), message_UID(message_UID){

  from = "";
  subject = "";
  body = "";

  auto set = mailimap_set_new_single(message_UID);
  auto fetch_type = mailimap_fetch_type_new_fetch_att_list_empty();
  auto env_att = mailimap_fetch_att_new_envelope();

  check_error(mailimap_fetch_type_new_fetch_att_list_add(fetch_type, env_att),
              "ERROR! Unable to fetch data");	      

  auto section = mailimap_section_new(NULL);
  auto body_att = mailimap_fetch_att_new_body_peek_section(section);

  check_error(mailimap_fetch_type_new_fetch_att_list_add(fetch_type, body_att),
	      "ERROR! Unable to fetch data");

  clist* fetch_result;
  check_error(mailimap_uid_fetch(session->imapSession, set, fetch_type, &fetch_result),
              "ERROR! Unable to fetch UID");

  auto msg_att = (mailimap_msg_att*) clist_content(clist_begin(fetch_result));

  // Assign From, Subject and Body
  for (clistiter* cur = clist_begin(msg_att->att_list); cur != NULL; cur = clist_next(cur)){

    auto item = (mailimap_msg_att_item*) clist_content(cur);
    auto item_static = item->att_data.att_static;
    auto item_envelope = item_static->att_data.att_env;

    if (item->att_type != MAILIMAP_MSG_ATT_ITEM_STATIC){
      continue;
    }
    
    if (item_static->att_type == MAILIMAP_MSG_ATT_ENVELOPE){
      
      if (item_envelope->env_subject){
	subject = item_envelope->env_subject;
      } else if (item_envelope->env_from->frm_list){
	fromList(item_envelope->env_from->frm_list);
      }
      
    }
    if (item_static->att_type == MAILIMAP_MSG_ATT_BODY_SECTION){

      body = item_static->att_data.att_body_section->sec_body_part;
    }
  }
  
  // Free memory
  mailimap_fetch_list_free(fetch_result);
  mailimap_fetch_type_free(fetch_type);
  mailimap_set_free(set);
}

string IMAP::Message::fromList(clist* from_lst){

  for (clistiter* i = clist_begin(from_lst); i != NULL; i = clist_next(i)){

    auto address = (mailimap_address*) clist_content(i);

    if (address->ad_mailbox_name &&
	address->ad_host_name){
      
      if(address->ad_personal_name){

	from += address->ad_personal_name + string(" ");
      }
      
      from +=
	string("<")+
	address->ad_mailbox_name +
	string("@") +
	address->ad_host_name +
	string(">");
    }
    
    from += "; ";
  }
}

string IMAP::Message::getBody(){

  return body;
}

string IMAP::Message::getField(string fieldname){

  if (fieldname == "From"){

    return from;

  } else if (fieldname == "Subject"){

    return subject;
  }

  return NULL;
}

void IMAP::Message::deleteFromMailbox(){

  auto set = mailimap_set_new_single(message_UID);
  auto flag_list = mailimap_flag_list_new_empty();
  auto flag = mailimap_flag_new_deleted();
  auto store_att_flags = mailimap_store_att_flags_new_set_flags(flag_list);

  check_error(mailimap_flag_list_add(flag_list, flag),
	      "ERROR! Unable to add delete flag to message");
  
  check_error(mailimap_uid_store(session->imapSession, set, store_att_flags),
	      "ERROR! Unable to update selected message flag");
  
  check_error(mailimap_expunge(session->imapSession),
	      "ERROR! Unable to delete selected message");
  
  mailimap_set_free(set);
  mailimap_store_att_flags_free(store_att_flags);
  
  for (int i = 0; session->messages[i]; i++) {
    delete session->messages[i];
    session->messages[i] = NULL;
  }
  
  session->updateUI();
}
