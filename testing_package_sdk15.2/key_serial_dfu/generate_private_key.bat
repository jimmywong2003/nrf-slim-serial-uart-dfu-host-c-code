@echo generate the private key
nrfutil keys generate demo_private.key

pause

@echo generate the public_key from private key
nrfutil keys display --key pk --format code demo_private.key --out_file demo_public_key.c
