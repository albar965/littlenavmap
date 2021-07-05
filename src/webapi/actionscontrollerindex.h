#ifndef ACTIONSCONTROLLERINDEX_H
#define ACTIONSCONTROLLERINDEX_H

/**
 * @brief class providing static QMetaType registration to keep
 * action controller declarations separated from WebApiController.
 * Available action controllers must be reqistered inside ::registerQMetaTypes
 */
class ActionsControllerIndex
{
public:
    static void registerQMetaTypes();
};

#endif // ACTIONSCONTROLLERINDEX_H
