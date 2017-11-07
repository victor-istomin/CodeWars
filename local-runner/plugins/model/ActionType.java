package model;

/**
 * Возможные действия игрока.
 * <p>
 * Игрок не может совершить новое действие, если в течение последних {@code game.actionDetectionInterval - 1} игровых
 * тиков он уже совершил максимально возможное для него количество действий. В начале игры это ограничение для каждого
 * игрока равно {@code game.baseActionCount}. Ограничение увеличивается за каждый контролируемый игроком центр
 * управления ({@code FacilityType.CONTROL_CENTER}).
 * <p>
 * Большинство действий требует указания дополнительных параметров, являющихся полями объекта {@code move}. В случае,
 * если эти параметры установлены некорректно либо указаны не все обязательные параметры, действие будет проигнорировано
 * игровым симулятором. Любое действие, отличное от {@code NONE}, даже проигнорированное, будет учтено в счётчике
 * действий игрока.
 */
public enum ActionType {
    /**
     * Ничего не делать.
     */
    NONE,

    /**
     * Пометить юнитов, соответствующих некоторым параметрам, как выделенных.
     * При этом, со всех остальных юнитов выделение снимается.
     * Юниты других игроков автоматически исключаются из выделения.
     */
    CLEAR_AND_SELECT,

    /**
     * Пометить юнитов, соответствующих некоторым параметрам, как выделенных.
     * При этом, выделенные ранее юниты остаются выделенными.
     * Юниты других игроков автоматически исключаются из выделения.
     */
    ADD_TO_SELECTION,

    /**
     * Снять выделение с юнитов, соответствующих некоторым параметрам.
     */
    DESELECT,

    /**
     * Установить для выделенных юнитов принадлежность к группе.
     */
    ASSIGN,

    /**
     * Убрать у выделенных юнитов принадлежность к группе.
     */
    DISMISS,

    /**
     * Расформировать группу.
     */
    DISBAND,

    /**
     * Приказать выделенным юнитам меремещаться в указанном направлении.
     */
    MOVE,

    /**
     * Приказать выделенным юнитам поворачиваться относительно некоторой точки.
     */
    ROTATE,

    /**
     * Настроить производство нужного типа техники на заводе ({@code FacilityType.VEHICLE_FACTORY}).
     */
    SETUP_VEHICLE_PRODUCTION
}
