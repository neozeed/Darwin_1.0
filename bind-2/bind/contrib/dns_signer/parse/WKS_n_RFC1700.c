/* $Header: /cvs/Darwin/Services/bind/bind/contrib/dns_signer/parse/WKS_n_RFC1700.c,v 1.1.1.1 1999/10/04 22:25:52 wsanchez Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <arpa/nameser.h>

struct port_number
{
	char				*pn_service;
	short				pn_port;
	char				*pn_protocol;
	struct port_number	*pn_next;
};

static struct port_number	*all_ports = NULL;
static int					already_tried = 0;

/* Assume the input file is trimmed from the RFC (ie no page headers) and
   has lines like this:

>                   0/tcp    Reserved
>                   0/udp    Reserved
> #                          Jon Postel <postel@isi.edu>
> tcpmux            1/tcp    TCP Port Service Multiplexer
> tcpmux            1/udp    TCP Port Service Multiplexer

*/
int read_in_port_numbers()
{
	FILE				*ports;
	char				filename[512];
	char				line[80];
	int					line_index;
	int					temp_index;
	char				temp_name[80];
	char				temp_protocol[80];
	struct port_number *temp;

	sprintf (filename, "%s/well-known-port-numbers", SORTER_DIR);
	ports = fopen (filename, "r");

	if (ports==NULL)
	{
		printf ("Couldn't open the file of well-known port assignments\n");
		printf ("Looking for %s\n", filename);
		already_tried = 1;
		return -1;
	}

	do
	{
		memset (line, 0, sizeof(line));
		fgets (line, sizeof(line), ports);
		if (feof(ports)) break;

		line_index = 0;
		if (line[line_index]=='#') continue;
		if (line[line_index]=='\n') continue;

		if (all_ports==NULL)
		{
			all_ports=(struct port_number*)malloc(sizeof(struct port_number));
			all_ports->pn_service = NULL;
			all_ports->pn_port = -1;
			all_ports->pn_protocol = NULL;
			all_ports->pn_next = NULL;
			temp = all_ports;
		}
		else
		{
			temp->pn_next=(struct port_number*)
								malloc(sizeof(struct port_number));
			temp->pn_next->pn_service = NULL;
			temp->pn_next->pn_port = -1;
			temp->pn_next->pn_protocol = NULL;
			temp->pn_next->pn_next = NULL;
			temp = temp->pn_next;
		}

		temp_index = 0;
		memset (temp_name, 0, sizeof(temp_name));
		while (!isspace(line[line_index]))
			temp_name[temp_index++]=line[line_index++];
		temp->pn_service = (char*) malloc (temp_index+1);
		strcpy (temp->pn_service, temp_name);
		
		while (isspace(line[line_index])) line_index++;

		temp->pn_port = (short) atoi (&line[line_index]);

		while (isdigit(line[line_index++]));

		temp_index = 0;
		memset (temp_protocol, 0, sizeof(temp_protocol));
		while (!isspace(line[line_index]))
			temp_protocol[temp_index++]=line[line_index++];
		temp->pn_protocol = (char*) malloc (temp_index+1);
		strcpy (temp->pn_protocol, temp_protocol);
	} while (!feof (ports));

	fclose (ports);
	return 0;
}

int	getservicenumber (char* service, char* protocol, u_int16_t *port)
{
	struct port_number	*temp;

	if (all_ports==NULL)
		if (already_tried == 0)
		{
			if (read_in_port_numbers()==-1) return -1;
		}
		else
			return -1;

	temp = all_ports;

	while(temp)
	{
		if (!strcasecmp (temp->pn_service, service) &&
				!strcasecmp (temp->pn_protocol, protocol))
		{
			*port = temp->pn_port;
			return 0;
		}
		temp = temp->pn_next;
	}
	
	return -1;
}

void free_port_numbers()
{
	struct port_number	*temp;

	while (all_ports)
	{
		temp = all_ports->pn_next;
		if (all_ports->pn_service) free (all_ports->pn_service);
		if (all_ports->pn_protocol) free (all_ports->pn_protocol);
		free (all_ports);
		all_ports = temp;
	}
}

/* Portions Copyright (c) 1995,1996,1997,1998 by Trusted Information Systems, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND TRUSTED INFORMATION SYSTEMS DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL TRUSTED INFORMATION 
 * SYSTEMS BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 *
 * Trusted Information Systems, Inc. has received approval from the
 * United States Government for export and reexport of TIS/DNSSEC
 * software from the United States of America under the provisions of
 * the Export Administration Regulations (EAR) General Software Note
 * (GSN) license exception for mass market software.  Under the
 * provisions of this license, this software may be exported or
 * reexported to all destinations except for the embargoed countries of
 * Cuba, Iran, Iraq, Libya, North Korea, Sudan and Syria.  Any export
 * or reexport of TIS/DNSSEC software to the embargoed countries
 * requires additional, specific licensing approval from the United
 * States Government. 
 */




