


int ReceiveMessage()
{
    Message msg;
    int tag, sender;

    ReceiveAny(&msg, &tag, &sender);

    if (sender == processId)
    {
        Error("Received message from self, tag: %d", tag);
        return -1;
    }

    switch (tag)
    {
        case TAG_ENQUEUE:
        {
            lockQueues();
            for (int company = 0; company < nCompanies; company++)
            {
                QueueAdd(&(Queues[company]), sender);
            }
            unlockQueues();
            break;
        }

        case TAG_CANCEL:
        {
            MessageCancel* msgCancel = (MessageCancel*) &(msg.data.data);
            Queue* queue = Queues[msgCancel->company];
            QueueRemove(queue, sender);
            CheckCancel(msgCancel->company);
            break;
        }

        case TAG_REQUEST:
        {
            MessageRequest* msgRequest = (MessageRequest*) &(msg.data.data);
            if (state == REQUESTING &&
                msgRequest->company == requestedCompany &&
                msgRequest->killer == requestedKiller)          // conflict
            {
                if (sender < processId)
                {
                    AbortRequest();
                    SendAck(ACK_OK);
                }
                else
                {
                    SendAck(ACK_REJECT);
                }
            }
            else if ((state == CONFIRMED || state == INPROGRESS) &&
                (msgRequest->company == requestedCompany &&
                msgRequest->killer == requestedKiller))
            {
                SendAck(ACK_REJECT);
            }
            // TODO note in Queues or somewhere ???
            break;
        }

        case TAG_ACK:
        {
            if (state == REQUESTING)
            {
                MessageAck* msgAck = (MessageAck*) &(msg.data.data);
                if (msgAck->ack == ACK_OK)
                {
                    AckReceived[sender] = 1;
                    // XXX signal cond. variable
                }
                else if (msgAck->ack == ACK_REJECT)
                {
                    assert(AckReceived[sender] == 0);
                    AbortRequest();
                }
                else
                {
                    Error("Received invalid ACK value: %d from %d",
                        msgAck->ack, sender);
                }
            }
            break;
        }

        case TAG_TAKE:
        {
            MessageTake* msgTake = (MessageTake*) &(msg.data.data);
            Killers[msgTake->company][msgTake->killer] = KILLER_BUSY;
            QueueRemove(Queues[msgTake->company], sender);
            break;
        }

        case TAG_RELEASE:
        {
            MessageRelease* msgRelease = (MessageRelease*) &(msg.data.data);
            Killers[msgRelease->company][msgRelease->killer] = KILLER_FREE;
            break;
        }

        case TAG_REVIEW:
        {
            MessageReview* msgReview = (MessageReview*) &(msg.data.data);
            HandleReview(msgReview);
            break;
        }

        case default:
        {
            Error("Received message of invalid type: %d", tag);
        }
    }
    return tag;
}
