/*  Copyright (C) 2018 CZ.NIC, z.s.p.o. <knot-dns@labs.nic.cz>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file edns_keepalive.c
 * @brief Minimalistic EDNS keepalive implementation on server side.
 *        If keepalive option is present in query,
 *        always reply with constant timeout value.
 *
 */
#include <libknot/packet/pkt.h>
#include "daemon/worker.h"
#include "lib/module.h"
#include "lib/layer.h"

static int edns_keepalive_finalize(kr_layer_t *ctx)
{
	struct kr_request *req = ctx->req;
	knot_pkt_t *answer = req->answer;
	const knot_rrset_t *src_opt = req->qsource.packet->opt_rr;
	knot_rrset_t *answ_opt = answer->opt_rr;

	const bool ka_want =
		req->qsource.flags.tcp &&
		src_opt != NULL &&
		knot_edns_get_option(src_opt, KNOT_EDNS_OPTION_TCP_KEEPALIVE) &&
		answ_opt != NULL;
	if (!ka_want) {
		return ctx->state;
	}
	const struct worker_ctx *worker = (const struct worker_ctx *)req->daemon_context;
	assert(worker);
	const struct network *net = &worker->engine->net;
	uint64_t timeout = net->tcp.in_idle_timeout / 100;
	if (timeout > UINT16_MAX) {
		timeout = UINT16_MAX;
	}
	knot_mm_t *pool = &answer->mm;
	uint16_t ka_size = knot_edns_keepalive_size(timeout);
	uint8_t ka_buf[ka_size];
	int ret = knot_edns_keepalive_write(ka_buf, ka_size, timeout);
	if (ret == KNOT_EOK) {
		ret = knot_edns_add_option(answ_opt, KNOT_EDNS_OPTION_TCP_KEEPALIVE,
					   ka_size, ka_buf, pool);
	}
	if (ret != KNOT_EOK) {
		ctx->state = KR_STATE_FAIL;
	}
	return ctx->state;
}

KR_EXPORT
const kr_layer_api_t *edns_keepalive_layer(struct kr_module *module)
{
	static kr_layer_api_t _layer = {
		.answer_finalize = &edns_keepalive_finalize,
	};
	/* Store module reference */
	_layer.data = module;
	return &_layer;
}

KR_MODULE_EXPORT(edns_keepalive);

