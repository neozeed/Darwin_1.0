class _IOFramebufferNotifier : public IONotifier
{
    friend IOFramebuffer;

    OSDeclareDefaultStructors(_IOFramebufferNotifier)

public:
    OSSet *				whence;

    IOFramebufferNotificationHandler	handler;
    OSObject *				self;
    void *				ref;
    bool				fEnable;

    virtual void remove();
    virtual bool disable();
    virtual void enable( bool was );
};
