#ifndef CAPTCHA_H
#define CAPTCHA_H

#include "persistence.h"

struct captcha *random_captcha();
void generate_captchas();
void invalidate_captcha(struct captcha *captcha);
void replace_captcha(struct captcha *captcha);

#endif // CAPTCHA_H
