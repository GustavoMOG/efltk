#include "etranslate.h"

void ETranslate::load_pot(FILE *fp)
{
    browser->begin();

	Fl_String encoding("iso8859-1");
	bool encoding_found = false;

    Fl_String token, msgid, msgstr, comment;
    bool msgid_found = false, msgstr_found = false;
    int c=0;
    bool skip=false;
    while(!feof(fp)) {
        if(c==0) c = fgetc(fp);
        token += c;

        if(c=='\n')
            token.clear();

        if(token=="#: ") {
            if(comment.length()>0)
                comment += ' ';
            while(!feof(fp)) {
                c=fgetc(fp);
                if(c=='\n') {
                    break;
                }
                comment += c;
            }
            continue;
        }

        if(token=="msgid ") {
            msgid.clear();
            fgetc(fp);
            skip=false;
            while(!feof(fp)) {
                c=fgetc(fp);
                if(!skip && c=='\"') {
                    fgetc(fp);
                    c = fgetc(fp);
                    if(c!='\"')
                        break;
                    continue;
                }
                skip = c=='\\';
                msgid += c;
            }
            token.clear();
            if(msgid.length()>0)
                msgid_found = true;
            continue;
        }

        if(token=="msgstr ") {
            msgstr.clear();
            fgetc(fp);
            skip=false;
            while(!feof(fp)) {
                c=fgetc(fp);
                if(!skip && c=='\"') {
                    fgetc(fp);
                    c = fgetc(fp);
                    if(c!='\"')
                        break;
                    continue;
                }
                skip = c=='\\';
                msgstr += c;
            }
            token.clear();
            if(msgid_found)
				msgstr_found = true;

			if(!encoding_found) {
				int pos = msgstr.pos("charset");
				if(pos>0) {
					pos += 8;
					int pos2 = msgstr.pos("\\n", pos);
					encoding = msgstr.sub_str(pos, pos2-pos);
					encoding_found = true;
				}
			}

            continue;
        }

        if(msgid_found && msgstr_found) {
            TranslateItem *i = new TranslateItem();
            i->orig(msgid);
            i->tr(Fl_String::from_codeset(encoding, msgstr.c_str(), msgstr.length()));
            i->comment(comment);
            i->finished(msgstr.length()>0);
            msgid_found = false;
            msgstr_found = false;
            comment.clear();
        }
		
        c=0;
    }

    browser->end();
}
